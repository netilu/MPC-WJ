#include "../src/apps/mplayerc/ThumbnailLayout.h"
#include <cassert>
#include <climits>
#include <iostream>

int main() {
    ThumbnailLayout l;
    assert(l.Calculate(1024,4,4,10,16,9,true,true,true,20,16));
    assert(l.cellWidth == 243 && l.cellHeight == 136 && l.height == 718);
    assert(l.Calculate(1024,4,4,10,9,16,false,false,false,20,16));
    assert(l.headerHeight == 0 && l.height == 1778);
    // Display aspect ratio, not coded 720x576 pixels, determines cell height.
    assert(l.Calculate(1024,2,2,0,16,9,false,false,false,20,16));
    assert(l.cellWidth == 512 && l.cellHeight == 288 && l.height == 576);
    assert(!l.Calculate(256,20,10,100,16,9,true,true,true,20,16));
    assert(!l.Calculate(255,1,1,0,1,1,false,false,false,20,16));
    assert(!l.Calculate(5121,1,1,0,1,1,false,false,false,20,16));
    assert(!l.Calculate(5120,20,1,0,1,100,false,false,false,20,16));
    assert(!l.Calculate(5120,20,1,0,1,INT_MAX,false,false,false,20,16));
    assert(!l.Calculate(256,1,10,0,16,9,false,false,true,20,96));
    assert(!l.Calculate(1024,0,1,0,16,9,false,false,false,20,16));
    assert(!l.Calculate(1024,1,11,0,16,9,false,false,false,20,16));
    assert(!l.Calculate(1024,1,1,-1,16,9,false,false,false,20,16));
    assert(!l.Calculate(1024,1,1,0,0,9,false,false,false,20,16));
    assert(!l.Calculate(256,1,1,100,16,9,true,false,false,20,16));
    assert(l.Calculate(5120,1,1,0,1,1,false,false,false,20,16));
    assert(l.bytes == 104857600);
    assert(l.Calculate(5120,5,1,0,1,1,false,false,false,20,16));
    assert(l.bytes == 524288000);
    assert(!l.Calculate(5120,6,1,0,1,1,false,false,false,20,16));
    assert(ThumbnailName(L"C:\\中文\\movie.mp4",L".png") == L"movie.mp4_thumbs.png");
    assert(ThumbnailName(L"C:/a/movie.mp4",L".jpg",2) == L"movie.mp4_thumbs (2).jpg");
    assert(ThumbnailName(L"clip",L".bmp") == L"clip_thumbs.bmp");
    std::cout << "Thumbnail layout and naming: passed\n";
}
