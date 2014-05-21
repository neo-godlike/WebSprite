//
//  CCInterlacedPngImage.h
//  cocos2d_tests
//
//  Created by sachin on 5/10/14.
//
//

#ifndef COCOS2D_TESTS_CCInterlacedPngImage_H_
#define COCOS2D_TESTS_CCInterlacedPngImage_H_

#include "cocos2d.h"
#include "png.h"

namespace cocos2d {

class InterlacedPngImage : public cocos2d::Image {
 public:
  bool initWithFilePath(const char* file_path);
  bool initWithFilePath(const char* local_file_path, const char* remote_file_path);
	/*thread safe*/
	void setImageHeader(size_t width, size_t height, int image_color_type, int out_channel);
	void setImageBodyData(char* data, size_t data_size);

 private:
  bool initWithPngData(const unsigned char * data, ssize_t dataLen);
};
  
} // namespace cocos2d

#endif //COCOS2D_TESTS_CCInterlacedPngImage_H_
