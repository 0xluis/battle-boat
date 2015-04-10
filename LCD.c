/*
 *
 * Name         :  pcd8544.c
 *
 * Description  :  This is a driver for the PCD8544 graphic LCD.
 *                 Based on the code written by Sylvain Bissonette
 *                 This driver is buffered in 504 bytes memory be sure
 *                 that your MCU having bigger memory
 *
 * Author       :  Fandi Gunawan <fandigunawan@gmail.com>
 * Website      :  http://fandigunawan.wordpress.com
 *
 * Credit       :  Sylvain Bissonette (2003)
 *
 * License      :  GPL v. 3
 *
 * Compiler     :  WinAVR, GCC for AVR platform
 *                 Tested version :
 *                 - 20070525 (avr-libc 1.4)
 *                 - 20071221 (avr-libc 1.6)
 *                 - 20081225 tested by Jakub Lasinski
 *                 - other version please contact me if you find out it is working
 * Compiler note:  Please be aware of using older/newer version since WinAVR
 *                 is under extensive development. Please compile with parameter -O1
 *
 * History      :
 * Version 0.2.6 (March 14, 2009) additional optimization by Jakub Lasinski
 * + Optimization using Memset and Memcpy
 * + Bug fix and sample program reviewed
 * + Commented <stdio.h>
 * Version 0.2.5 (December 25, 2008) x-mas version :)
 * + Boundary check is added (reported by starlino on Dec 20, 2008)
 * + Return value is added, it will definitely useful for error checking
 * Version 0.2.4 (March 5, 2008)
 * + Multiplier was added to LcdBars to scale the bars
 * Version 0.2.3 (February 29, 2008)
 * + Rolled back LcdFStr function because of serious bug
 * + Stable release
 * Version 0.2.2 (February 27, 2008)
 * + Optimizing LcdFStr function
 * Version 0.2.1 (January 2, 2008)
 * + Clean up codes
 * + All settings were migrated to pcd8544.h
 * + Using _BV() instead of << to make a better readable code
 * Version 0.2 (December 11-14, 2007)
 * + Bug fixed in LcdLine() and LcdStr()
 * + Adding new routine
 *    - LcdFStr()
 *    - LcdSingleBar()
 *    - LcdBars()
 *    - LcdRect()
 *    - LcdImage()
 * + PROGMEM used instead of using.data section
 * Version 0.1 (December 3, 2007)
 * + First stable driver
 *
 * Note         :
 * Font will be displayed this way (16x6)
 * 1 2 3 4 5 6 7 8 9 0 1 2 3 4
 * 2
 * 3
 * 4
 * 5
 * 6
 *
 * Contributor : 
 * + Jakub Lasinski
 */

#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "LCD.h"

/* Function prototypes */

static void LcdSend    ( byte data, LcdCmdData cd );
static void Delay      ( void );

/* Global variables */

/* Cache buffer in SRAM 84*48 bits or 504 bytes */
static byte LcdCache [ LCD_CACHE_SIZE ];

/* Cache index */
static int LcdCacheIdx;

/* Lower part of water mark */
static int LoWaterMark;

/* Higher part of water mark */
static int HiWaterMark;

/* Variable to decide whether update Lcd Cache is active/nonactive */
static bool UpdateLcd;

/*
 * Name         :  LcdInit
 * Description  :  Performs MCU SPI & LCD controller initialization.
 * Argument(s)  :  None.
 * Return value :  None.
 */
void LcdInit ( void )
{
    /* Pull-up on reset pin. */
    LCD_PORT |= _BV ( LCD_RST_PIN );

    /* Set output bits on LCD Port. */
    LCD_DDR |= _BV( LCD_RST_PIN ) | _BV( LCD_DC_PIN ) | _BV( LCD_CE_PIN ) | _BV( SPI_MOSI_PIN ) | _BV( SPI_CLK_PIN );

    Delay();

    /* Toggle display reset pin. */
    LCD_PORT &= ~( _BV( LCD_RST_PIN ) );
    Delay();
    LCD_PORT |= _BV ( LCD_RST_PIN );

    /* Enable SPI port:
    * No interrupt, MSBit first, Master mode, CPOL->0, CPHA->0, Clk/4
    */
    SPCR = 0x50;

    /* Disable LCD controller */
    LCD_PORT |= _BV( LCD_CE_PIN );

    LcdSend( 0x21, LCD_CMD ); /* LCD Extended Commands. */
    LcdSend( 0xC8, LCD_CMD ); /* Set LCD Vop (Contrast).*/
    LcdSend( 0x06, LCD_CMD ); /* Set Temp coefficent. */
    LcdSend( 0x13, LCD_CMD ); /* LCD bias mode 1:48. */
    LcdSend( 0x20, LCD_CMD ); /* LCD Standard Commands,Horizontal addressing mode */
    LcdSend( 0x0C, LCD_CMD ); /* LCD in normal mode. */

    /* Reset watermark pointers to empty */
    LoWaterMark = LCD_CACHE_SIZE;
    HiWaterMark = 0;

    /* Clear display on first time use */
    LcdClear();
    LcdUpdate();
}

/*
 * Name         :  LcdContrast
 * Description  :  Set display contrast.
 * Argument(s)  :  contrast -> Contrast value from 0x00 to 0x7F.
 * Return value :  None.
 */
void LcdContrast ( byte contrast )
{
    /* LCD Extended Commands. */
    LcdSend( 0x21, LCD_CMD );

    /* Set LCD contrast level. */
    LcdSend( 0x80 | contrast, LCD_CMD );

    /* LCD Standard Commands, horizontal addressing mode. */
    LcdSend( 0x20, LCD_CMD );
}

/*
 * Name         :  LcdClear
 * Description  :  Clears the display. LcdUpdate must be called next.
 * Argument(s)  :  None.
 * Return value :  None.
 * Note         :  Based on Sylvain Bissonette's code
 */
void LcdClear ( void )
{

	memset(LcdCache,0x00,LCD_CACHE_SIZE); //Sugestion - its faster and its 10 bytes less in program mem
    /* Reset watermark pointers to full */
    LoWaterMark = 0;
    HiWaterMark = LCD_CACHE_SIZE - 1;

    /* Set update flag to be true */
    UpdateLcd = TRUE;
}


/*
 * Name         :  LcdPixel
 * Description  :  Displays a pixel at given absolute (x, y) location.
 * Argument(s)  :  x, y -> Absolute pixel coordinates
 *                 mode -> Off, On or Xor. See enum in pcd8544.h.
 * Return value :  see return value on pcd8544.h
 * Note         :  Based on Sylvain Bissonette's code
 */
byte LcdPixel ( byte x, byte y, LcdPixelMode mode )
{
    word  index;
    byte  offset;
    byte  data;

    /* Prevent from getting out of border */
    if ( x > LCD_X_RES ) return OUT_OF_BORDER;
    if ( y > LCD_Y_RES ) return OUT_OF_BORDER;

    /* Recalculating index and offset */
    index = ( ( y / 8 ) * 84 ) + x;
    offset  = y - ( ( y / 8 ) * 8 );

    data = LcdCache[ index ];

    /* Bit processing */

	/* Clear mode */
    if ( mode == PIXEL_OFF )
    {
        data &= ( ~( 0x01 << offset ) );
    }

    /* On mode */
    else if ( mode == PIXEL_ON )
    {
        data |= ( 0x01 << offset );
    }

    /* Xor mode */
    else if ( mode  == PIXEL_XOR )
    {
        data ^= ( 0x01 << offset );
    }

    /* Final result copied to cache */
    LcdCache[ index ] = data;

    if ( index < LoWaterMark )
    {
        /*  Update low marker. */
        LoWaterMark = index;
    }

    if ( index > HiWaterMark )
    {
        /*  Update high marker. */
        HiWaterMark = index;
    }
    return OK;
}


/*
 * Name         :  LcdRect
 * Description  :  Display a rectangle.
 * Argument(s)  :  x1   -> absolute first x axis coordinate
 *                 y1   -> absolute first y axis coordinate
 *				   x2   -> absolute second x axis coordinate
 *				   y2   -> absolute second y axis coordinate
 *				   mode -> Off, On or Xor. See enum in pcd8544.h.
 * Return value :  see return value on pcd8544.h.
 */
byte LcdRect ( byte x1, byte x2, byte y1, byte y2, LcdPixelMode mode )
{
	byte tmpIdxX,tmpIdxY;
    byte response;

	/* Checking border */
	if ( ( x1 > LCD_X_RES ) ||  ( x2 > LCD_X_RES ) || ( y1 > LCD_Y_RES ) || ( y2 > LCD_Y_RES ) )
		/* If out of border then return */
		return OUT_OF_BORDER;

	if ( ( x2 > x1 ) && ( y2 > y1 ) )
	{
		for ( tmpIdxY = y1; tmpIdxY < y2; tmpIdxY++ )
		{
			/* Draw line horizontally */
			for ( tmpIdxX = x1; tmpIdxX < x2; tmpIdxX++ )
            {
				/* Draw a pixel */
				response = LcdPixel( tmpIdxX, tmpIdxY, mode );
                if(response)
                    return response;
            }
		}

		/* Set update flag to be true */
		UpdateLcd = TRUE;
	}
    return OK;
}
/*
 * Name         :  LcdImage
 * Description  :  Image mode display routine.
 * Argument(s)  :  Address of image in hexes
 * Return value :  None.
 * Example      :  LcdImage(&sample_image_declared_as_array);
 */
void LcdImage ( const byte *imageData )
{
	/* Initialize cache index to 0 */
//	LcdCacheIdx = 0;
//	/* While within cache range */
//    for ( LcdCacheIdx = 0; LcdCacheIdx < LCD_CACHE_SIZE; LcdCacheIdx++ )
//    {
//		/* Copy data from pointer to cache buffer */
//        LcdCache[LcdCacheIdx] = pgm_read_byte( imageData++ );
//    }
	/* optimized by Jakub Lasinski, version 0.2.6, March 14, 2009 */
    memcpy_P(LcdCache,imageData,LCD_CACHE_SIZE);	//Same as aboeve - 6 bytes less and faster instruction
	/* Reset watermark pointers to be full */
    LoWaterMark = 0;
    HiWaterMark = LCD_CACHE_SIZE - 1;

	/* Set update flag to be true */
    UpdateLcd = TRUE;
}

/*
 * Name         :  LcdImage2
 * Description  :  Image mode display routine...since battle boat has a array that needs to be read and written to
 *				   the array cannot be written in ROM therfore needed to change memcpy_P
 * Argument(s)  :  Address of image in hexes
 * Return value :  None.
 * Example      :  LcdImage(&sample_image_declared_as_array);
 */
void LcdImage2 ( byte *imageData )
{

    memcpy(LcdCache,imageData,LCD_CACHE_SIZE);	//Same as aboeve - 6 bytes less and faster instruction
	/* Reset watermark pointers to be full */
    LoWaterMark = 0;
    HiWaterMark = LCD_CACHE_SIZE - 1;

	/* Set update flag to be true */
    UpdateLcd = TRUE;
}

/*
 * Name         :  LcdUpdate
 * Description  :  Copies the LCD cache into the device RAM.
 * Argument(s)  :  None.
 * Return value :  None.
 */
void LcdUpdate ( void )
{
    int i;

    if ( LoWaterMark < 0 )
        LoWaterMark = 0;
    else if ( LoWaterMark >= LCD_CACHE_SIZE )
        LoWaterMark = LCD_CACHE_SIZE - 1;

    if ( HiWaterMark < 0 )
        HiWaterMark = 0;
    else if ( HiWaterMark >= LCD_CACHE_SIZE )
        HiWaterMark = LCD_CACHE_SIZE - 1;

    /*  Set base address according to LoWaterMark. */
    LcdSend( 0x80 | ( LoWaterMark % LCD_X_RES ), LCD_CMD );
    LcdSend( 0x40 | ( LoWaterMark / LCD_X_RES ), LCD_CMD );

    /*  Serialize the display buffer. */
    for ( i = LoWaterMark; i <= HiWaterMark; i++ )
    {
        LcdSend( LcdCache[ i ], LCD_DATA );
    }

    /*  Reset watermark pointers. */
    LoWaterMark = LCD_CACHE_SIZE - 1;
    HiWaterMark = 0;

    /* Set update flag to be true */
	UpdateLcd = FALSE;
}

/*
 * Name         :  LcdSend
 * Description  :  Sends data to display controller.
 * Argument(s)  :  data -> Data to be sent
 *                 cd   -> Command or data (see enum in pcd8544.h)
 * Return value :  None.
 */
static void LcdSend ( byte data, LcdCmdData cd )
{
    /*  Enable display controller (active low). */
    LCD_PORT &= ~( _BV( LCD_CE_PIN ) );

    if ( cd == LCD_DATA )
    {
        LCD_PORT |= _BV( LCD_DC_PIN );
    }
    else
    {
        LCD_PORT &= ~( _BV( LCD_DC_PIN ) );
    }

    /*  Send data to display controller. */
    SPDR = data;

    /*  Wait until Tx register empty. */
    while ( (SPSR & 0x80) != 0x80 );


    /* Disable display controller. */
    LCD_PORT |= _BV( LCD_CE_PIN );
}

//we need draw to the "Canvas board" as opposed to directly to the LCD
//this is done so we can get the X or O on the board and have it
//be persistent since the board img stored is ROM
byte LcdPixel2 ( byte x, byte y, LcdPixelMode mode, unsigned char* board)
{
	word  index;
	byte  offset;
	byte  data;

	/* Prevent from getting out of border */
	if ( x > LCD_X_RES ) return OUT_OF_BORDER;
	if ( y > LCD_Y_RES ) return OUT_OF_BORDER;

	/* Recalculating index and offset */
	index = ( ( y / 8 ) * 84 ) + x;
	offset  = y - ( ( y / 8 ) * 8 );

	data = board[ index ];

	/* Bit processing */

	/* Clear mode */
	if ( mode == PIXEL_OFF )
	{
		data &= ( ~( 0x01 << offset ) );
	}

	/* On mode */
	else if ( mode == PIXEL_ON )
	{
		data |= ( 0x01 << offset );
	}

	/* Xor mode */
	else if ( mode  == PIXEL_XOR )
	{
		data ^= ( 0x01 << offset );
	}

	/* Final result copied to cache */
	board[ index ] = data;
	return OK;
}

//draws an O to an array with x1 y1 being top left pixel
void drawO(unsigned char x1, unsigned char y1, unsigned char* board)
{
	LcdPixel2(x1, y1, PIXEL_XOR,board);
	LcdPixel2(x1+1, y1+1, PIXEL_XOR,board);
	LcdPixel2(x1+1, y1+2, PIXEL_XOR,board);
	LcdPixel2(x1, y1+3, PIXEL_XOR,board);
	LcdPixel2(x1+3, y1, PIXEL_XOR,board);
	LcdPixel2(x1+2, y1+1, PIXEL_XOR,board);
	LcdPixel2(x1+2, y1+2, PIXEL_XOR,board);
	LcdPixel2(x1+3, y1+3, PIXEL_XOR,board);
}

//draws an X to an arry with x1 y1 being top left pixel
void drawX(unsigned char x1, unsigned char y1, unsigned char* board)
{
	LcdPixel2(x1+1, y1, PIXEL_XOR,board);
	LcdPixel2(x1+2, y1, PIXEL_XOR,board);
	LcdPixel2(x1+3, y1+1, PIXEL_XOR,board);
	LcdPixel2(x1, y1+1, PIXEL_XOR,board);
	LcdPixel2(x1, y1+2, PIXEL_XOR,board);
	LcdPixel2(x1+1, y1+3, PIXEL_XOR,board);
	LcdPixel2(x1+2, y1+3, PIXEL_XOR,board);
	LcdPixel2(x1+3, y1+2, PIXEL_XOR,board);
}
/*
 * Name         :  Delay
 * Description  :  Uncalibrated delay for LCD init routine.
 * Argument(s)  :  None.
 * Return value :  None.
 */
static void Delay ( void )
{
    int i;

    for ( i = -32000; i < 32000; i++ );
}


