//
//  http_HttpConnection.cpp
//  cocos2d_tests
//
//  Created by sachin on 5/11/14.
//
//

#include "http_connection.h"

namespace constants {
const int curl_timeout_read = 30;
const int curl_timeout_connect = 60;
}

namespace {

// Callback function used by libcurl for collect response data
size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream) {
  std::vector<char> *recvBuffer = (std::vector<char>*)stream;
  size_t sizes = size * nmemb;
  
  // add data to the end of recvBuffer
  // write data maybe called more than once in a single request
  recvBuffer->insert(recvBuffer->end(), (char*)ptr, (char*)ptr+sizes);
  
  return sizes;
}
} //namespace anonymous

HttpConnection::HttpConnection(): easy_handle_(NULL), response_code_(0){

}

HttpConnection::~HttpConnection() {
	if (easy_handle_) {
		curl_easy_cleanup(easy_handle_);
		easy_handle_ = NULL;
	}
}

bool HttpConnection::Init(const char* url) {
  if (easy_handle_) {
    curl_easy_cleanup(easy_handle_);
    easy_handle_ = NULL;
  }
  easy_handle_ = curl_easy_init();
  if (!easy_handle_) {
    return false;
  }
  return (setCurlOption(CURLOPT_URL, url)
          && setCurlOption(CURLOPT_ERRORBUFFER, error_bufffer)
          && setCurlOption(CURLOPT_TIMEOUT, constants::curl_timeout_read)
          && setCurlOption(CURLOPT_CONNECTTIMEOUT, constants::curl_timeout_connect)
          && setCurlOption(CURLOPT_SSL_VERIFYPEER, 0L)
          && setCurlOption(CURLOPT_SSL_VERIFYHOST, 0L));
}

void HttpConnection::SetWriteCallBack(void *ptr, WrietFunctionType write_fun) {
  setCurlOption(CURLOPT_WRITEFUNCTION, write_fun);
  setCurlOption(CURLOPT_WRITEDATA, ptr);
}


bool HttpConnection::PerformGet() {
  CURLcode code = curl_easy_perform(easy_handle_);
  
  if (CURLE_OK != code) {
    ReportCurlErrorCode(code);
    return false;
  }

  code = curl_easy_getinfo(easy_handle_, CURLINFO_RESPONSE_CODE, &response_code_);
  if (code != CURLE_OK) {
    ReportCurlErrorCode(code);
    return false;
  }
  if ( response_code_ != 200) {
    return false;
  }

  return true;
}

bool HttpConnection::PerformPost(const char* post_data, int post_data_size,
                                               int* response_code ) {
  if (!(setCurlOption(CURLOPT_POST, 1)
        && setCurlOption(CURLOPT_POSTFIELDS, post_data)
        && setCurlOption(CURLOPT_POSTFIELDSIZE, post_data_size))) {
    return false;
  }
  
  CURLcode code = curl_easy_perform(easy_handle_);
  if (CURLE_OK != code) {
    ReportCurlErrorCode(code);
    return false;
  }
  
  code = curl_easy_getinfo(easy_handle_, CURLINFO_RESPONSE_CODE, response_code);
  if ( *response_code != 200) {
    return false;
  }
  
  return true;
}


void HttpConnection::ReportCurlErrorCode(int error_code) {
}