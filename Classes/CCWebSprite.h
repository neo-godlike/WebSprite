//
//  CCWebSprite.h
//  
//
//  Created by sachin on 5/13/14.
//
//

#ifndef CCWEBSPRITE_H_
#define CCWEBSPRITE_H_

#include <mutex>
#include <vector>
#include <functional>

#include "cocos2d.h"


class HttpConnection;
namespace util {
class PNGCodec;
}
namespace cocos2d {

class InterlacedPngImage;
class WebSprite : public cocos2d::Sprite {
 public:
  WebSprite();
  virtual ~WebSprite();
  
  static WebSprite* create();
  static WebSprite* createWithFileUrl(const char* file_url);
  
  bool initWithFileUrl(const char* file_url);

 private:
	 class DataBridge {
	  public:
		 // Callback function used by libcurl for collect response data
		 static size_t WriteData(void *ptr, size_t size, size_t nmemb, void *stream);
		 static void ReadHeaderCompleteCallBack(void* ptr);
		 static void ReadRowCompleteCallBack(void* ptr, int pass);
		 static void ReadAllCompleteCallBack(void* ptr);
	 };
	
	friend class DataBridge;

  virtual void update(float fDelta);
  void updateTexture();
	void reciverData(unsigned char* data, size_t data_size);
	void readHeaderComplete();
  void readRowComplete(int pass);
  void readAllComplete();
	bool initWithRemoteFile();
	bool initWithLocalFile();
	bool isRemotoeFileUrl(const char *file_url);

 private:
  std::shared_ptr<HttpConnection> http_connection_;
	std::shared_ptr<util::PNGCodec>     png_coder_;
  std::vector<std::function<void ()> > perform_main_thread_functions_;
  std::mutex perform_mutex_;
	InterlacedPngImage* interlaced_png_image_buff_;
  std::string file_url_;
  int code_pass_;
 private:
  CC_DISALLOW_COPY_AND_ASSIGN(WebSprite)
};
  
} // namespace cocos2d

#endif // CCWEBSPRITE_H_
