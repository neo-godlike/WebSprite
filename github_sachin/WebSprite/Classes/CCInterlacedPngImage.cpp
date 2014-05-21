//
//  CCInterlacedPngImage.cpp
//  cocos2d_tests
//
//  Created by sachin on 5/11/14.
//
//

#include "CCInterlacedPngImage.h"
namespace cocos2d {

namespace {
struct GimpImage{
  int  	 width;
  int  	 height;
  int  	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char 	 pixel_data[8 * 8 * 4 + 1];
}null_content_image = {
  1, 1, 3,
  "\275\275\275"
};

} //namespace anonymous

bool InterlacedPngImage::initWithFilePath(const char *file_path) {
//  if (isRemoteFilePath(file_path)) {
//    initWithRemoteFilePath(file_path);
//    return true;
//  }
  bool ret = false;
  _filePath = cocos2d::FileUtils::getInstance()->fullPathForFilename(file_path);
  
#ifdef EMSCRIPTEN
  // Emscripten includes a re-implementation of SDL that uses HTML5 canvas
  // operations underneath. Consequently, loading images via IMG_Load (an SDL
  // API) will be a lot faster than running libpng et al as compiled with
  // Emscripten.
  SDL_Surface *iSurf = IMG_Load(fullPath.c_str());
  
  int size = 4 * (iSurf->w * iSurf->h);
  ret = initWithRawData((const unsigned char*)iSurf->pixels, size, iSurf->w, iSurf->h, 8, true);
  
  unsigned int *tmp = (unsigned int *)_data;
  int nrPixels = iSurf->w * iSurf->h;
  for(int i = 0; i < nrPixels; i++)
  {
    unsigned char *p = _data + i * 4;
    tmp[i] = CC_RGB_PREMULTIPLY_ALPHA( p[0], p[1], p[2], p[3] );
  }
  
  SDL_FreeSurface(iSurf);
#else
  cocos2d::Data data = cocos2d::FileUtils::getInstance()->getDataFromFile(_filePath);
  
  if (!data.isNull())
  {
    ret = initWithPngData(data.getBytes(), data.getSize());
  }
#endif // EMSCRIPTEN
  
  return ret;
}

bool InterlacedPngImage::initWithFilePath(const char *local_file_path, const char *remote_file_path) {
  return true;
}

bool InterlacedPngImage::initWithPngData(const unsigned char * data, ssize_t dataLen) {
  Image::initWithRawData(null_content_image.pixel_data, null_content_image.width * null_content_image.height * null_content_image.bytes_per_pixel, null_content_image.width, null_content_image.height,8);
  return true;
}

void InterlacedPngImage::setImageHeader(size_t width, size_t height, int image_color_type, int out_channel) {
	_width = width;
  _height = height;
	png_uint_32 color_type = (png_uint_32)image_color_type;
	switch (color_type)
	{
	case PNG_COLOR_TYPE_GRAY:
		_renderFormat = cocos2d::Texture2D::PixelFormat::I8;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		_renderFormat = cocos2d::Texture2D::PixelFormat::AI88;
		break;
	case PNG_COLOR_TYPE_RGB:
		_renderFormat = cocos2d::Texture2D::PixelFormat::RGB888;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		_renderFormat = cocos2d::Texture2D::PixelFormat::RGBA8888;
		break;
	default:
		break;
	}

   	_dataLen = _width * _height * out_channel * sizeof(unsigned char);
	_data = static_cast<unsigned char*>(malloc(_dataLen + 1));
	memset(_data, 0, _dataLen);
}

void InterlacedPngImage::setImageBodyData(char* data, size_t data_size) {
	memcpy(_data, data, _dataLen);
}

} // namespace cocos2d


