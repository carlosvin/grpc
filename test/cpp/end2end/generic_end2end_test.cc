/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <chrono>
#include <memory>

#include "src/cpp/proto/proto_utils.h"
#include "src/cpp/util/time.h"
#include "test/core/util/port.h"
#include "test/core/util/test_config.h"
#include "test/cpp/util/echo.pb.h"
#include <grpc++/async_generic_service.h>
#include <grpc++/async_unary_call.h>
#include <grpc++/byte_buffer.h>
#include <grpc++/channel_arguments.h>
#include <grpc++/channel_interface.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/credentials.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/server_credentials.h>
#include <grpc++/slice.h>
#include <grpc++/status.h>
#include <grpc++/stream.h>
#include <gtest/gtest.h>

#include <grpc/grpc.h>
#include <grpc/support/thd.h>
#include <grpc/support/time.h>

using grpc::cpp::test::util::EchoRequest;
using grpc::cpp::test::util::EchoResponse;
using std::chrono::system_clock;

namespace grpc {
namespace testing {
namespace {

void* tag(int i) { return (void*)(gpr_intptr) i; }

void verify_ok(CompletionQueue* cq, int i, bool expect_ok) {
  bool ok;
  void* got_tag;
  EXPECT_TRUE(cq->Next(&got_tag, &ok));
  EXPECT_EQ(expect_ok, ok);
  EXPECT_EQ(tag(i), got_tag);
}

bool ParseFromByteBuffer(ByteBuffer* buffer, grpc::protobuf::Message* message) {
  std::vector<Slice> slices;
  buffer->Dump(&slices);
  grpc::string buf;
  buf.reserve(buffer->Length());
  for (const Slice& s : slices) {
    buf.append(reinterpret_cast<const char*>(s.begin()), s.size());
  }
  return message->ParseFromString(buf);
}

class GenericEnd2endTest : public ::testing::Test {
 protected:
  GenericEnd2endTest() : generic_service_("*") {}

  void SetUp() GRPC_OVERRIDE {
    int port = grpc_pick_unused_port_or_die();
    server_address_ << "localhost:" << port;
    // Setup server
    ServerBuilder builder;
    builder.AddListeningPort(server_address_.str(), InsecureServerCredentials());
    builder.RegisterAsyncGenericService(&generic_service_);
    server_ = builder.BuildAndStart();
  }

  void TearDown() GRPC_OVERRIDE {
    server_->Shutdown();
    void* ignored_tag;
    bool ignored_ok;
    cli_cq_.Shutdown();
    srv_cq_.Shutdown();
    while (cli_cq_.Next(&ignored_tag, &ignored_ok))
      ;
    while (srv_cq_.Next(&ignored_tag, &ignored_ok))
      ;
  }

  void ResetStub() {
    std::shared_ptr<ChannelInterface> channel = CreateChannel(
        server_address_.str(), InsecureCredentials(), ChannelArguments());
    stub_ = std::move(grpc::cpp::test::util::TestService::NewStub(channel));
  }

  void server_ok(int i) { verify_ok(&srv_cq_, i, true); }
  void client_ok(int i) { verify_ok(&cli_cq_, i, true); }
  void server_fail(int i) { verify_ok(&srv_cq_, i, false); }
  void client_fail(int i) { verify_ok(&cli_cq_, i, false); }

  void SendRpc(int num_rpcs) {
    for (int i = 0; i < num_rpcs; i++) {
      EchoRequest send_request;
      EchoRequest recv_request;
      EchoResponse send_response;
      EchoResponse recv_response;
      Status recv_status;

      ClientContext cli_ctx;
      GenericServerContext srv_ctx;
      GenericServerAsyncReaderWriter stream(&srv_ctx);

      send_request.set_message("Hello");
      std::unique_ptr<ClientAsyncResponseReader<EchoResponse> > response_reader(
          stub_->AsyncEcho(&cli_ctx, send_request, &cli_cq_, tag(1)));
      client_ok(1);

      generic_service_.RequestCall(&srv_ctx, &stream, &srv_cq_, tag(2));

      verify_ok(generic_service_.completion_queue(), 2, true);
      EXPECT_EQ(server_address_.str(), srv_ctx.host());
      EXPECT_EQ("/grpc.cpp.test.util.TestService/Echo", srv_ctx.method());
      ByteBuffer recv_buffer;
      stream.Read(&recv_buffer, tag(3));
      server_ok(3);
      EXPECT_TRUE(ParseFromByteBuffer(&recv_buffer, &recv_request));
      EXPECT_EQ(send_request.message(), recv_request.message());

      send_response.set_message(recv_request.message());
      grpc::string buf;
      send_response.SerializeToString(&buf);
      gpr_slice s = gpr_slice_from_copied_string(buf.c_str());
      Slice slice(s, Slice::STEAL_REF);
      ByteBuffer send_buffer(&slice, 1);
      stream.Write(send_buffer, tag(4));
      server_ok(4);

      stream.Finish(Status::OK, tag(5));
      server_ok(5);

      response_reader->Finish(&recv_response, &recv_status, tag(4));
      client_ok(4);

      EXPECT_EQ(send_response.message(), recv_response.message());
      EXPECT_TRUE(recv_status.IsOk());
    }
  }

  CompletionQueue cli_cq_;
  CompletionQueue srv_cq_;
  std::unique_ptr<grpc::cpp::test::util::TestService::Stub> stub_;
  std::unique_ptr<Server> server_;
  AsyncGenericService generic_service_;
  std::ostringstream server_address_;
};

TEST_F(GenericEnd2endTest, SimpleRpc) {
  ResetStub();
  SendRpc(1);
}

TEST_F(GenericEnd2endTest, SequentialRpcs) {
  ResetStub();
  SendRpc(10);
}

// One ping, one pong.
TEST_F(GenericEnd2endTest, SimpleBidiStreaming) {
  ResetStub();

  EchoRequest send_request;
  EchoRequest recv_request;
  EchoResponse send_response;
  EchoResponse recv_response;
  Status recv_status;
  ClientContext cli_ctx;
  GenericServerContext srv_ctx;
  GenericServerAsyncReaderWriter srv_stream(&srv_ctx);

  send_request.set_message("Hello");
  std::unique_ptr<ClientAsyncReaderWriter<EchoRequest, EchoResponse> >
      cli_stream(stub_->AsyncBidiStream(&cli_ctx, &cli_cq_, tag(1)));
  client_ok(1);

  generic_service_.RequestCall(&srv_ctx, &srv_stream, &srv_cq_, tag(2));

  verify_ok(generic_service_.completion_queue(), 2, true);
  EXPECT_EQ(server_address_.str(), srv_ctx.host());
  EXPECT_EQ("/grpc.cpp.test.util.TestService/BidiStream", srv_ctx.method());

  cli_stream->Write(send_request, tag(3));
  client_ok(3);

  ByteBuffer recv_buffer;
  srv_stream.Read(&recv_buffer, tag(4));
  server_ok(4);
  EXPECT_TRUE(ParseFromByteBuffer(&recv_buffer, &recv_request));
  EXPECT_EQ(send_request.message(), recv_request.message());

  send_response.set_message(recv_request.message());
  grpc::string buf;
  send_response.SerializeToString(&buf);
  gpr_slice s = gpr_slice_from_copied_string(buf.c_str());
  Slice slice(s, Slice::STEAL_REF);
  ByteBuffer send_buffer(&slice, 1);
  srv_stream.Write(send_buffer, tag(5));
  server_ok(5);

  cli_stream->Read(&recv_response, tag(6));
  client_ok(6);
  EXPECT_EQ(send_response.message(), recv_response.message());

  cli_stream->WritesDone(tag(7));
  client_ok(7);

  recv_buffer.Clear();
  srv_stream.Read(&recv_buffer, tag(8));
  server_fail(8);

  srv_stream.Finish(Status::OK, tag(9));
  server_ok(9);

  cli_stream->Finish(&recv_status, tag(10));
  client_ok(10);

  EXPECT_EQ(send_response.message(), recv_response.message());
  EXPECT_TRUE(recv_status.IsOk());
}

}  // namespace
}  // namespace testing
}  // namespace grpc

int main(int argc, char** argv) {
  grpc_test_init(argc, argv);
  grpc_init();
  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  grpc_shutdown();
  google::protobuf::ShutdownProtobufLibrary();
  return result;
}
