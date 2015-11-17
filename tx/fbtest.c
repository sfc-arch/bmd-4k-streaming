/*
 * fbtst.c 
 *    2006.7.19 Kensuke Ooyu
 */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <sys/mman.h>

#define DEVICE_NAME "/dev/fb0"
#define DIV_BYTE 8

#define X_PIXEL_MAX 320
#define Y_LINE_MAX  240

#define BORDER1 80
#define BORDER2 160

#define COLOR_RED    0xf800
#define COLOR_GREEN  0x07e0
#define COLOR_BLUE   0x001f
#define COLOR_WHITE  0xffff
#define COLOR_BLACK  0x0000
#define COLOR_YELLOW 0xffe0

/* function prototype */
void send_current_error_msg(char *ptr);
void send_current_information(char *ptr);

int main(void)
{
    int fd_framebuffer ;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize ;
    long int location;
    char *fbptr ;
    char tmp[DIV_BYTE*10];

    int x , y ;
    int xres,yres,vbpp,line_len;
    unsigned short tcolor ;

    /* 読み書き用にファイルを開く */
    fd_framebuffer = open( DEVICE_NAME , O_RDWR);
    if ( !fd_framebuffer ) {
        send_current_error_msg("Framebuffer device open error !");
        exit(1);
    }
    send_current_information("The framebuffer device was opened !");

    /* 固定スクリーン情報取得 */
    if ( ioctl( fd_framebuffer , FBIOGET_FSCREENINFO , &finfo ) ) {
        send_current_error_msg("Fixed information not gotton !");
        exit(2);
    }

    /* 変動スクリーン情報取得 */
    if ( ioctl( fd_framebuffer , FBIOGET_VSCREENINFO , &vinfo ) ) {
        send_current_error_msg("Variable information not gotton !");
        exit(3);
    }
    xres = vinfo.xres ;
    yres = vinfo.yres ;
    vbpp = vinfo.bits_per_pixel ;
    line_len = finfo.line_length ;
    sprintf( tmp , "%d(pixel)x%d(line), %dbpp(bits per pixel)",xres,yres,vbpp);
    send_current_information( tmp );

    /* バイト単位でのスクリーンのサイズを計算 */
    screensize = xres * yres * vbpp / DIV_BYTE ;

    /* デバイスをメモリにマップする */
    fbptr = (char *)mmap(0,screensize,PROT_READ | PROT_WRITE,MAP_SHARED,fd_framebuffer,0);
    if ( (int)fbptr == -1 ) {
        send_current_error_msg("Don't get framebuffer device to memory !");
        exit(4);
    }
    send_current_information("The framebuffer device was mapped !");

    /* 表示 */
    for ( y = 0 ; y < Y_LINE_MAX ; y++ ) {
        /* 色決定 */
        tcolor = COLOR_BLUE ;
        if ( y > BORDER2 ) {
            tcolor = COLOR_RED ;
        } else {
            if ( y > BORDER1 ) {
                tcolor = COLOR_GREEN ;
            }
        }
        /* １ライン処理 */
        for ( x = 0 ; x < X_PIXEL_MAX ; x++ ) {
            /* 格納位置計算 */
            location = ((x+vinfo.xoffset) * vbpp / DIV_BYTE) + (y+vinfo.yoffset) * line_len ;
            /* 着色 */
            *((unsigned short *)(fbptr + location)) = tcolor;
        }
    }
    munmap(fbptr,screensize);
    close(fd_framebuffer);
    return 0;
}

void send_current_error_msg(char *ptr)
{
    fprintf( stderr , "%s\n" , ptr );
}

void send_current_information(char *ptr)
{
    fprintf( stdout , "%s\n" , ptr );

}
