//
//  CCWebSprite.cpp
//  cocos2d_tests
//
//  Created by sachin on 5/13/14.
//
//

#include "CCWebSprite.h"
#include "CCInterlacedPngImage.h"
#include "http_connection.h"
#include "png_codec.h"

namespace cocos2d {

namespace {
  // Callback function used by libcurl for collect response data
size_t WriteData(void *ptr, size_t size, size_t nmemb, void *stream) {
  if (stream == nullptr) {
		return 0;
  }
  WebSprite* web_sprite = static_cast<WebSprite*>(stream);
	if (web_sprite == nullptr) {
		return 0;
	}
  size_t sizes = size * nmemb;
  web_sprite->reciverData((unsigned  char*)ptr, sizes);
  return sizes;
}

void ReadHeaderCompleteCallBack(void* ptr) {
	WebSprite* web_sprite = static_cast<WebSprite*>(ptr);
	web_sprite->readHeaderComplete();
}

void ReadRowCompleteCallBack(void* ptr, int pass) {
  WebSprite* web_sprite = static_cast<WebSprite*>(ptr);
  web_sprite->readRowComplete(pass);
}

void ReadAllCompleteCallBack(void* ptr) {
  WebSprite* web_sprite = static_cast<WebSprite*>(ptr);
  web_sprite->readAllComplete();
}
} //namespace anymous
  
WebSprite::WebSprite(): http_connection_(new HttpConnection()),
	png_coder_(new util::PNGCodec()), interlaced_png_image_buff_(new InterlacedPngImage()), code_pass_(-1){

}

WebSprite::~WebSprite() {
	http_connection_->SetWriteCallBack(nullptr, WriteData);
	CC_SAFE_DELETE(png_coder_);
	CC_SAFE_RELEASE(interlaced_png_image_buff_);
}

WebSprite* WebSprite::create() {
  WebSprite *sprite = new WebSprite();
  if (sprite && sprite->init())
  {
    sprite->autorelease();
    return sprite;
  }
  CC_SAFE_DELETE(sprite);
  return nullptr;
}

WebSprite* WebSprite::createWithFileUrl(const char *file_url) {
  WebSprite *sprite = new WebSprite();
  if (sprite && sprite->initWithFileUrl(file_url))
  {
    sprite->autorelease();
    return sprite;
  }
  CC_SAFE_DELETE(sprite);
  return nullptr;
}

bool WebSprite::initWithFileUrl(const char *file_url) {
  Sprite::init();
  file_url_ = file_url;
  http_connection_->Init(file_url);
  png_coder_->PrepareDecode();
	png_coder_->SetReadCallBack(this, ReadHeaderCompleteCallBack, ReadRowCompleteCallBack, ReadAllCompleteCallBack);
  http_connection_->SetWriteCallBack(this, WriteData);
  this->scheduleUpdate();
  std::thread http_thread = std::thread(std::bind(&HttpConnection::PerformGet, http_connection_));
  http_thread.detach();
  return true;
}

void WebSprite::reciverData(unsigned char* data, size_t data_size) {
	png_coder_->Decoding(data, data_size);
}

void WebSprite::updateTexture() {
  cocos2d::Texture2D* texture = cocos2d::Director::getInstance()->getTextureCache()->addImage(interlaced_png_image_buff_, file_url_);
	texture->updateWithData(interlaced_png_image_buff_->getData(), 0, 0, interlaced_png_image_buff_->getWidth(),
				interlaced_png_image_buff_->getHeight());
	SpriteFrame* sprite_frame = cocos2d::SpriteFrame::createWithTexture(texture,
			CCRectMake(0,0,texture->getContentSize().width, texture->getContentSize().height));
	Sprite::setSpriteFrame(sprite_frame);
}

void WebSprite::readHeaderComplete() {
	interlaced_png_image_buff_->setImageHeader(png_coder_->png_width(), png_coder_->png_height(), png_coder_->png_color_type(), png_coder_->png_output_channels());
}

void WebSprite::readRowComplete(int pass) {
  if (code_pass_ < pass) {
    perform_mutex_.lock();
		interlaced_png_image_buff_->setImageBodyData((char*)png_coder_->png_data_buffer(), png_coder_->png_data_size());
    perform_main_thread_functions_.push_back(std::bind(&WebSprite::updateTexture, this));
    perform_mutex_.unlock();
    code_pass_ = pass;
  }
}

// run on sub thread
void WebSprite::readAllComplete() {
  perform_mutex_.lock();
	interlaced_png_image_buff_->setImageBodyData((char*)png_coder_->png_data_buffer(), png_coder_->png_data_size());
  perform_main_thread_functions_.push_back(std::bind(&WebSprite::updateTexture, this));
  perform_mutex_.unlock();
}

void WebSprite::update(float fDelta) {
  Sprite::update(fDelta);
  perform_mutex_.lock();
  for (std::vector<std::function<void ()> >::iterator it = perform_main_thread_functions_.begin();
       it != perform_main_thread_functions_.end(); ++it) {
    (*it)();
  } 
  perform_main_thread_functions_.clear();
  perform_mutex_.unlock();
}
  
  
} // namespace cocos2d