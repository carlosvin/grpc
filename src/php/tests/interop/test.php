<?php
// DO NOT EDIT! Generated by Protobuf-PHP protoc plugin 1.0
// Source: test/cpp/interop/test.proto
//   Date: 2015-01-30 23:30:46

namespace grpc\testing {

  class TestServiceClient{

    private $rpc_impl;

    public function __construct($rpc_impl) {
      $this->rpc_impl = $rpc_impl;
    }
    /**
     * @param grpc\testing\EmptyMessage $input
     */
    public function EmptyCall(\grpc\testing\EmptyMessage $argument, $metadata = array()) {
      return $this->rpc_impl->_simpleRequest('/grpc.testing.TestService/EmptyCall', $argument, '\grpc\testing\EmptyMessage::deserialize', $metadata);
    }
    /**
     * @param grpc\testing\SimpleRequest $input
     */
    public function UnaryCall(\grpc\testing\SimpleRequest $argument, $metadata = array()) {
      return $this->rpc_impl->_simpleRequest('/grpc.testing.TestService/UnaryCall', $argument, '\grpc\testing\SimpleResponse::deserialize', $metadata);
    }
    /**
     * @param grpc\testing\StreamingOutputCallRequest $input
     */
    public function StreamingOutputCall($argument, $metadata = array()) {
      return $this->rpc_impl->_serverStreamRequest('/grpc.testing.TestService/StreamingOutputCall', $argument, '\grpc\testing\StreamingOutputCallResponse::deserialize', $metadata);
    }
    /**
     * @param grpc\testing\StreamingInputCallRequest $input
     */
    public function StreamingInputCall($arguments, $metadata = array()) {
      return $this->rpc_impl->_clientStreamRequest('/grpc.testing.TestService/StreamingInputCall', $arguments, '\grpc\testing\StreamingInputCallResponse::deserialize', $metadata);
    }
    /**
     * @param grpc\testing\StreamingOutputCallRequest $input
     */
    public function FullDuplexCall($metadata = array()) {
      return $this->rpc_impl->_bidiRequest('/grpc.testing.TestService/FullDuplexCall', '\grpc\testing\StreamingOutputCallResponse::deserialize', $metadata);
    }
    /**
     * @param grpc\testing\StreamingOutputCallRequest $input
     */
    public function HalfDuplexCall($metadata = array()) {
      return $this->rpc_impl->_bidiRequest('/grpc.testing.TestService/HalfDuplexCall', '\grpc\testing\StreamingOutputCallResponse::deserialize', $metadata);
    }
  }
}
