/*

  u8x8_d_sh1122.c
  
  Universal 8bit Graphics Library (https://github.com/olikraus/u8g2/)

  Copyright (c) 2016, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list 
    of conditions and the following disclaimer.
    
  * Redistributions in binary form must reproduce the above copyright notice, this 
    list of conditions and the following disclaimer in the documentation and/or other 
    materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  


  Idea: SH1122 is a horizontal device, which doesn't support u8x8
  However in the similar SSD1362 device, we do the correct tile conversion,
    so maybe takeover code from SSD1362 to SH1122, so that SH1122 can also
    support u8x8

  
*/
#include "u8x8.h"
#include "u8g2.h"
#include <string.h>




static const uint8_t u8x8_d_sh1122_powersave0_seq[] = {
  U8X8_START_TRANSFER(),             	/* enable chip, delay is part of the transfer start */
  U8X8_C(0x0af),		                /* sh1122: display on */
  U8X8_END_TRANSFER(),             	/* disable chip */
  U8X8_END()             			/* end of sequence */
};

static const uint8_t u8x8_d_sh1122_powersave1_seq[] = {
  U8X8_START_TRANSFER(),             	/* enable chip, delay is part of the transfer start */
  U8X8_C(0x0ae),		                /* sh1122: display off */
  U8X8_END_TRANSFER(),             	/* disable chip */
  U8X8_END()             			/* end of sequence */
};




/*
  input:
    one tile (8 Bytes)
  output:
    Tile for SH1122 (32 Bytes)
*/

/*
static uint8_t u8x8_sh1122_to32_dest_buf[32];

static uint8_t *u8x8_sh1122_8to32(U8X8_UNUSED u8x8_t *u8x8, uint8_t *ptr)
{
  uint8_t v;
  uint8_t a,b;
  uint8_t i, j;
  uint8_t *dest;
  
  for( j = 0; j < 4; j++ )
  {
    dest = u8x8_sh1122_to32_dest_buf;
    dest += j;
    a =*ptr;
    ptr++;
    b = *ptr;
    ptr++;
    for( i = 0; i < 8; i++ )
    {
      v = 0;
      if ( a&1 ) v |= 0xf0;
      if ( b&1 ) v |= 0x0f;
      *dest = v;
      dest+=4;
      a >>= 1;
      b >>= 1;
    }
  }
  
  return u8x8_sh1122_to32_dest_buf;
}
*/


static uint8_t g_sh1122_fg_level = 0x0F; /* default maximum brightness */
static uint8_t g_sh1122_natural_active = 0;
static uint8_t g_sh1122_gray_fb[256*64/2]; /* 8192-byte 4bpp accumulation buffer */

void u8g2_sh1122_SetForegroundLevel(uint8_t level)
{
  g_sh1122_fg_level = level & 0x0F;
}

uint8_t u8g2_sh1122_GetForegroundLevel(void)
{
  return g_sh1122_fg_level & 0x0F;
}

/* Return 1 if the current u8g2 instance is using the SH1122 driver */
uint8_t u8g2_sh1122_IsDevice(u8g2_t *u8g2)
{
  if (u8g2 == NULL)
    return 0;
  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  if (u8x8 == NULL)
    return 0;
  /* compare display callback with our SH1122 device function */
  extern uint8_t u8x8_d_sh1122_256x64(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
  return (uint8_t)(u8x8->display_cb == u8x8_d_sh1122_256x64);
}

uint8_t u8g2_sh1122_IsNaturalGrayActive(void)
{
  return g_sh1122_natural_active;
}

void u8g2_sh1122_NaturalGrayBegin(u8g2_t *u8g2)
{
  (void)u8g2;
  u8g2_sh1122_GrayClear(g_sh1122_gray_fb, 0);
  g_sh1122_natural_active = 1;
}

void u8g2_sh1122_NaturalGrayFlushLayer(u8g2_t *u8g2, uint8_t level)
{
  if (!g_sh1122_natural_active) return;
  u8g2_sh1122_CompositePageTo4bpp(u8g2, g_sh1122_gray_fb, (uint8_t)(level & 0x0F), 0 /* overwrite */);
  u8g2_ClearBuffer(u8g2);
}

void u8g2_sh1122_NaturalGrayFinish(u8g2_t *u8g2, uint8_t level)
{
  if (!g_sh1122_natural_active) return;
  /* composite the last pending layer and send */
  u8g2_sh1122_CompositePageTo4bpp(u8g2, g_sh1122_gray_fb, (uint8_t)(level & 0x0F), 0 /* overwrite */);
  u8g2_sh1122_Send4bppBuffer(u8g2, g_sh1122_gray_fb);
  g_sh1122_natural_active = 0;
}

static uint8_t u8x8_write_byte_to_16gr_device(u8x8_t *u8x8, uint8_t b)
{
  static uint8_t buf[4];
  /* Expand 1bpp source byte into 4 data bytes for SH1122 RAM, using current 4-bit fg level */
  uint8_t lvl = g_sh1122_fg_level & 0x0F;
  uint8_t map0 = 0x00;            /* 00 -> both nibbles 0 */
  uint8_t map1 = (uint8_t)(lvl);  /* 01 -> low nibble = lvl */
  uint8_t map2 = (uint8_t)(lvl << 4); /* 10 -> high nibble = lvl */
  uint8_t map3 = (uint8_t)((lvl << 4) | lvl); /* 11 -> both nibbles = lvl */

  uint8_t v = (uint8_t)(b & 3);
  buf[3] = (v==0)?map0: (v==1?map1:(v==2?map2:map3));
  b >>= 2;
  v = (uint8_t)(b & 3);
  buf[2] = (v==0)?map0: (v==1?map1:(v==2?map2:map3));
  b >>= 2;
  v = (uint8_t)(b & 3);
  buf[1] = (v==0)?map0: (v==1?map1:(v==2?map2:map3));
  b >>= 2;
  v = (uint8_t)(b & 3);
  buf[0] = (v==0)?map0: (v==1?map1:(v==2?map2:map3));
  return u8x8_cad_SendData(u8x8, 4, buf);
}

uint8_t u8x8_d_sh1122_common(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint8_t x; 
  uint8_t y, c, i;
  uint8_t *ptr;
  switch(msg)
  {
    /* U8X8_MSG_DISPLAY_SETUP_MEMORY is handled by the calling function */
    /*
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      break;
    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_256x64_init_seq);
      break;
    */
    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
      if ( arg_int == 0 )
	u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_powersave0_seq);
      else
	u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_powersave1_seq);
      break;
#ifdef U8X8_WITH_SET_CONTRAST
    case U8X8_MSG_DISPLAY_SET_CONTRAST:
      u8x8_cad_StartTransfer(u8x8);
      u8x8_cad_SendCmd(u8x8, 0x081 );
      u8x8_cad_SendArg(u8x8, arg_int );	/* sh1122 has range from 0 to 255 */
      u8x8_cad_EndTransfer(u8x8);
      break;
#endif
    case U8X8_MSG_DISPLAY_DRAW_TILE:
      u8x8_cad_StartTransfer(u8x8);
      x = ((u8x8_tile_t *)arg_ptr)->x_pos;    
      x *= 2;		// 4 Mar 2022: probably this needs to be 4, but this device is call with x=0 only
      x += u8x8->x_offset;		
    
      y = (((u8x8_tile_t *)arg_ptr)->y_pos);
      y *= 8;
          
      
      c = ((u8x8_tile_t *)arg_ptr)->cnt;	/* number of tiles */
      ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;	/* data ptr to the tiles */
      for( i = 0; i < 8; i++ )
      {
	u8x8_cad_SendCmd(u8x8, 0x0b0 );	/* set row address */
	u8x8_cad_SendArg(u8x8, y);
	u8x8_cad_SendCmd(u8x8, x & 15 );	/* lower 4 bit*/
	u8x8_cad_SendCmd(u8x8, 0x010 | (x >> 4) );	/* higher 3 bit */	  
	c = ((u8x8_tile_t *)arg_ptr)->cnt;	/* number of tiles */

	while (  c > 0 )
	{
	  u8x8_write_byte_to_16gr_device(u8x8, *ptr);
	  c--;
	  ptr++;
	}
	y++;
      }

      
      u8x8_cad_EndTransfer(u8x8);
      break;
    default:
      return 0;
  }
  return 1;
}

/*=========================================================*/

static const uint8_t u8x8_d_sh1122_256x64_flip0_seq[] = {
  U8X8_START_TRANSFER(),             	/* enable chip, delay is part of the transfer start */
  U8X8_C(0x0a1),		/* remap */
  U8X8_C(0x0c8),		/* remap */
  U8X8_C(0x060),
  U8X8_END_TRANSFER(),             	/* disable chip */
  U8X8_END()             			/* end of sequence */
};

static const uint8_t u8x8_d_sh1122_256x64_flip1_seq[] = {
  U8X8_START_TRANSFER(),             	/* enable chip, delay is part of the transfer start */
  U8X8_C(0x0a0),		/* remap */
  U8X8_C(0x0c0),		/* remap */
  U8X8_C(0x040),
  U8X8_END_TRANSFER(),             	/* disable chip */
  U8X8_END()             			/* end of sequence */
};

static const u8x8_display_info_t u8x8_sh1122_256x64_display_info =
{
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 10,
  /* reset_pulse_width_ms = */ 10, 	/* sh1122: 10 us */
  /* post_reset_wait_ms = */ 20, 	/* */
  /* sda_setup_time_ns = */ 125,		/* sh1122: cycle time is 250ns, so use 250/2 */
  /* sck_pulse_width_ns = */ 125,	/* sh1122: cycle time is 250ns, so use 250/2 */
  /* sck_clock_hz = */ 40000000UL,	/* since Arduino 1.6.0, the SPI bus speed in Hz. Should be  1000000000/sck_pulse_width_ns  */
  /* spi_mode = */ 0,		/* active high, rising edge */
  /* i2c_bus_clock_100kHz = */ 4,
  /* data_setup_time_ns = */ 10,
  /* write_pulse_width_ns = */ 150,	/* sh1122: cycle time is 300ns, so use 300/2 = 150 */
  /* tile_width = */ 32,		/* 256 pixel, so we require 32 bytes for this */
  /* tile_height = */ 8,
  /* default_x_offset = */ 0,	/* this is the byte offset (there are two pixel per byte with 4 bit per pixel) */
  /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 256,
  /* pixel_height = */ 64
};


static const uint8_t u8x8_d_sh1122_256x64_init_seq[] = {
    
  U8X8_DLY(1),
  U8X8_START_TRANSFER(),             	/* enable chip, delay is part of the transfer start */
  U8X8_DLY(1),
  
  U8X8_C(0xae),		                /* display off */
  U8X8_C(0x40),				/* display start line */  
  U8X8_C(0x0a0),		/* remap */
  U8X8_C(0x0c0),		/* remap */
  U8X8_CA(0x81, 0x80),			/* set display contrast  */  
  U8X8_CA(0xa8, 0x3f),			/* multiplex ratio 1/64 Duty (0x0F~0x3F) */  
  U8X8_CA(0xad, 0x81),			/* use buildin DC-DC with 0.6 * 500 kHz */  
  
  U8X8_CA(0xd5, 0x50),			/* set display clock divide ratio (lower 4 bit)/oscillator frequency (upper 4 bit)  */  
  U8X8_CA(0xd3, 0x00),			/* display offset, shift mapping ram counter */  
  U8X8_CA(0xd9, 0x22),			/* pre charge (lower 4 bit) and discharge(higher 4 bit) period */  
  U8X8_CA(0xdb, 0x35),			/* VCOM deselect level */  
  U8X8_CA(0xdc, 0x35),			/* Pre Charge output voltage */  
  U8X8_C(0x030),				/* discharge level */

  U8X8_DLY(1),					/* delay  */

  U8X8_END_TRANSFER(),             	/* disable chip */
  U8X8_END()             			/* end of sequence */
};


uint8_t u8x8_d_sh1122_256x64(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &u8x8_sh1122_256x64_display_info);
      break;
    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_256x64_init_seq);
      break;
    case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
      if ( arg_int == 0 )
      {
	u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_256x64_flip0_seq);
	u8x8->x_offset = u8x8->display_info->default_x_offset;
      }
      else
      {
	u8x8_cad_SendSequence(u8x8, u8x8_d_sh1122_256x64_flip1_seq);
	u8x8->x_offset = u8x8->display_info->flipmode_x_offset;
      }
      break;
    
    default:
      return u8x8_d_sh1122_common(u8x8, msg, arg_int, arg_ptr);
  }
  return 1;
}


/*
  Send a full 4-bit-per-pixel grayscale frame buffer to SH1122.
  Buffer layout: 256x64 pixels, 4bpp, packed as two pixels per byte.
  - For x even: pixel in high nibble
  - For x odd:  pixel in low nibble
  Thus each scanline has 128 bytes, total buffer size is 8192 bytes.
*/
void u8g2_sh1122_Send4bppBuffer(u8g2_t *u8g2, const uint8_t *grayscale_buffer)
{
  if ( u8g2 == NULL || grayscale_buffer == NULL )
    return;

  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  if (u8x8 == NULL)
    return;

  /* Transmit the buffer row by row */
  u8x8_cad_StartTransfer(u8x8);
  for (uint8_t y = 0; y < 64; y++)
  {
    /* Set row address */
    u8x8_cad_SendCmd(u8x8, 0x0b0);
    u8x8_cad_SendArg(u8x8, y);

    /* Set column to 0 */
    u8x8_cad_SendCmd(u8x8, 0x00);   /* column low */
    u8x8_cad_SendCmd(u8x8, 0x010);  /* column high */

    /* Send 128 bytes for this row */
    u8x8_cad_SendData(u8x8, 128, (uint8_t *)(grayscale_buffer + (uint16_t)y * 128));
  }
  u8x8_cad_EndTransfer(u8x8);
}



/*
  Utilities to build a 4bpp framebuffer from the current u8g2 page buffer
  so that all standard u8g2 drawing APIs (text, shapes, bitmaps) can be used
  and rotation is respected automatically via the u8g2 page/rotation pipeline.
*/
void u8g2_sh1122_GrayClear(uint8_t *grayscale_buffer, uint8_t level)
{
  if (grayscale_buffer == NULL) return;
  uint8_t n = level & 0x0F;
  n = (uint8_t)((n << 4) | n);
  memset(grayscale_buffer, n, 256*64/2);
}

void u8g2_sh1122_CompositePageTo4bpp(u8g2_t *u8g2, uint8_t *grayscale_buffer, uint8_t fg_level, uint8_t mode)
{
  if (u8g2 == NULL || grayscale_buffer == NULL) return;
  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  if (u8x8 == NULL) return;

  uint8_t w = u8x8->display_info->tile_width;     /* bytes per scanline */
  uint8_t page_tile_rows = u8g2->tile_buf_height; /* number of tile rows in current page */
  uint16_t base_y = (uint16_t)u8g2->tile_curr_row * 8; /* starting pixel row for this page */

  uint8_t level = fg_level & 0x0F;

  /* iterate over each pixel row in the current page buffer */
  for (uint16_t r = 0; r < (uint16_t)page_tile_rows * 8; r++)
  {
    uint16_t dest_y = base_y + r;
    if (dest_y >= 64) break; /* safety */

    uint8_t *src = u8g2->tile_buf_ptr + r * w;

    /* each src byte encodes 8 horizontal pixels, msb is leftmost */
    for (uint16_t bx = 0; bx < w; bx++)
    {
      uint8_t b = src[bx];
      if (b == 0) continue; /* small speed-up */

      for (uint8_t i = 0; i < 8; i++)
      {
        if (b & (uint8_t)(0x80 >> i))
        {
          uint16_t x = (uint16_t)bx * 8u + i;
          if (x >= 256) continue;
          uint16_t off = (uint16_t)dest_y * 128u + (x >> 1);
          uint8_t cur = grayscale_buffer[off];
          if ((x & 1u) == 0)
          {
            /* even x: high nibble */
            uint8_t hi = (uint8_t)(cur >> 4);
            uint8_t newv = (mode == 1) ? (uint8_t)((hi > level) ? hi : level) : level;
            grayscale_buffer[off] = (uint8_t)((newv << 4) | (cur & 0x0F));
          }
          else
          {
            /* odd x: low nibble */
            uint8_t lo = (uint8_t)(cur & 0x0F);
            uint8_t newv = (mode == 1) ? (uint8_t)((lo > level) ? lo : level) : level;
            grayscale_buffer[off] = (uint8_t)((cur & 0xF0) | newv);
          }
        }
      }
    }
  }
}
