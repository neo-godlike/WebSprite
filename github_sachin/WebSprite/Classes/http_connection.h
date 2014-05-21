//
//  http_HttpConnection.h
//  cocos2d_tests
//
//  Created by sachin on 5/11/14.
//
//

#ifndef COCOS2D_TESTS_HTTP_HTTPCONNECTION_H_
#define COCOS2D_TESTS_HTTP_HTTPCONNECTION_H_

#include <vector>

#include "curl/curl.h"


class HttpConnection {
 public:
  HttpConnection();
  ~HttpConnection();
  bool Init(const char*url);
  bool PerformGet();
  bool PerformPost(const char* post_data, int post_data_size, int* response_code);
 
  typedef size_t (*WrietFunctionType)(void *ptr, size_t size, size_t nmemb, void *stream);
  
  void SetWriteCallBack(void* ptr, WrietFunctionType write_fun);
 private:
  template <class T>
  bool setCurlOption(CURLoption option, T data)
  {
    CURLcode code = curl_easy_setopt(easy_handle_, option, data);
    if (code != CURLE_OK)
    {
      ReportCurlErrorCode(code);
      return false;
    }
    return true;
  }
  void ReportCurlErrorCode(int error_code);
 private:
  CURL* easy_handle_;
  char error_bufffer[CURL_ERROR_SIZE];
  std::vector<char> receiver_buffer;
  int response_code_;
};

#endif //COCOS2D_TESTS_HTTP_HTTPCONNECTION_H_
