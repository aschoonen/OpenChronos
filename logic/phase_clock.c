// *************************************************************************************************
//
//	Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
//	Copyright (C) 2010 Daniel Poelzleithner
//	 
//	 
//	  Redistribution and use in source and binary forms, with or without 
//	  modification, are permitted provided that the following conditions 
//	  are met:
//	
//	    Redistributions of source code must retain the above copyright 
//	    notice, this list of conditions and the following disclaimer.
//	 
//	    Redistributions in binary form must reproduce the above copyright
//	    notice, this list of conditions and the following disclaimer in the 
//	    documentation and/or other materials provided with the   
//	    distribution.
//	 
//	    Neither the name of Texas Instruments Incorporated nor the names of
//	    its contributors may be used to endorse or promote products derived
//	    from this software without specific prior written permission.
//	
//	  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//	  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//	  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//	  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//	  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//	  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//	  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//	  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//	  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//	  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//	  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************
// SimpliciTI functions.
// *************************************************************************************************


// *************************************************************************************************
// Include section

// system
#include "project.h"

#ifdef CONFIG_PHASE_CLOCK

// driver
#include <string.h>
#include "display.h"
#include "vti_as.h"
#include "ports.h"
#include "timer.h"
#include "radio.h"

// logic
#include "acceleration.h"
#include "rfsimpliciti.h"
//#include "bluerobin.h"
#include "simpliciti.h"
#include "phase_clock.h"
#include "date.h"
#include "alarm.h"
#include "temperature.h"
#include "vti_ps.h"
#include "altitude.h"


// *************************************************************************************************
// Prototypes section
void simpliciti_get_data_callback(void);
void start_simpliciti_sleep();
void start_simpliciti_sync(void);


// *************************************************************************************************
// Defines section
#define TEST

// Each packet index requires 2 bytes, so we can have 9 packet indizes in 18 bytes usable payload
#define BM_SYNC_BURST_PACKETS_IN_DATA		(9u)


// *************************************************************************************************
// Global Variable section
struct SPhase sPhase;

// flag contains status information, trigger to send data and trigger to exit SimpliciTI
unsigned char phase_clock_flag;

// 4 data bytes to send 
unsigned char phase_clock_data[SIMPLICITI_MAX_PAYLOAD_LENGTH];

// 4 byte device address overrides SimpliciTI end device address set in "smpl_config.dat"
unsigned char phase_clock_ed_address[4];

// Length of data 
unsigned char phase_clock_payload_length;

// 1 = send one or more reply packets, 0 = no need to reply
//unsigned char simpliciti_reply;
unsigned char phase_clock_reply_count;

// 1 = send packets sequentially from burst_start to burst_end, 2 = send packets addressed by their index
//u8 		burst_mode;

// Start and end index of packets to send out
//u16		burst_start, burst_end;

// Array containing requested packets
//u16		burst_packet[BM_SYNC_BURST_PACKETS_IN_DATA];

// Current packet index
//u8		burst_packet_index;


// *************************************************************************************************
// Extern section
extern void (*fptr_lcd_function_line1)(u8 line, u8 update);


// *************************************************************************************************
// @fn          reset_rf
// @brief       Reset SimpliciTI data. 
// @param       none
// @return      none
// *************************************************************************************************
void reset_sleep(void)
{
	// No connection
	sPhase.mode = SLEEP_OFF;

	// reset rf
	reset_rf();
}


// *************************************************************************************************
// @fn          sx_sleep
// @brief       Start Sleep mode. Button DOWN connects/disconnects to access point.
// @param       u8 line		LINE2
// @return      none
// *************************************************************************************************
void sx_phase(u8 line)
{
	// Exit if battery voltage is too low for radio operation
	if (sys.flag.low_battery) return;

	// Exit if BlueRobin stack is active
#ifndef ELIMINATE_BLUEROBIN
	if (is_bluerobin()) return;
#endif
  	// Start SimpliciTI in tx only mode
	start_simpliciti_sleep();
}

inline u8 diff(u8 x1, u8 x2) {
    u8 b1 = x1 - x2;
    if(b1 > 128)
        b1 = x2 - x1;
    return b1;
}


// *************************************************************************************************
// @fn          phase_clock_calcpoint
// @brief       calculate one data point for the out buffer
// @param       none
// @return      none
// *************************************************************************************************
void phase_clock_calcpoint() {
    u16 x,y,z,res = 0;
    u8 i = 0;
    for(i=1;i<SLEEP_BUFFER-1;i++) {
        x += diff(sPhase.data[i-1][0], sPhase.data[i][0]);
        y += diff(sPhase.data[i-1][1], sPhase.data[i][1]);
        z += diff(sPhase.data[i-1][1], sPhase.data[i][1]);
    }
    // can't overflow when SLEEP_BUFFER is not larger then 171
    res = x + y + z;
    memcpy(&sPhase.out + sPhase.data_nr, &res, sizeof(u16));
    // reset stack index
    sPhase.data_nr += sizeof(u16);

}


// *************************************************************************************************
// @fn          start_simpliciti_tx_only
// @brief       Start SimpliciTI (tx only). 
// @param       simpliciti_state_t		SIMPLICITI_ACCELERATION, SIMPLICITI_BUTTONS
// @return      none
// *************************************************************************************************
void start_simpliciti_sleep()
{
  	// Display time in line 1
	clear_line(LINE1);  	
	fptr_lcd_function_line1(LINE1, DISPLAY_LINE_CLEAR);
	display_time(LINE1, DISPLAY_LINE_UPDATE_FULL);

	// Preset simpliciti_data with mode (key or mouse click) and clear other data bytes
    simpliciti_data[0] = SIMPLICITI_PHASE_CLOCK_EVENTS;
    /*
	simpliciti_data[1] = 0;
	simpliciti_data[2] = 0;
	simpliciti_data[3] = 0;
	*/
    //memset(&simpliciti_data, 0, SIMPLICITI_MAX_PAYLOAD_LENGTH);

    sPhase.data_nr = 0;
    sPhase.out_nr = 0;
    //memset(&simpliciti_data, 0, SIMPLICITI_MAX_PAYLOAD_LENGTH);
    
	// Turn on beeper icon to show activity
	display_symbol(LCD_ICON_BEEPER1, SEG_ON_BLINK_ON);
	display_symbol(LCD_ICON_BEEPER2, SEG_ON_BLINK_ON);
	display_symbol(LCD_ICON_BEEPER3, SEG_ON_BLINK_ON);

	// Debounce button event
	Timer0_A4_Delay(CONV_MS_TO_TICKS(BUTTONS_DEBOUNCE_TIME_OUT));
	
	// Prepare radio for RF communication
	open_radio();

	// Set SimpliciTI mode
	sRFsmpl.mode = SIMPLICITI_PHASE_CLOCK;
	
	// Set SimpliciTI timeout to save battery power
	//sRFsmpl.timeout = SIMPLICITI_TIMEOUT; 
		
	// Start SimpliciTI stack. Try to link to access point.
	// Exit with timeout or by a button DOWN press.
	if (simpliciti_link())
	{
		// Start acceleration sensor
		as_start();

		// Enter TX only routine. This will transfer button events and/or acceleration data to access point.
		simpliciti_main_tx_only();
	}

	// Set SimpliciTI state to OFF
	sRFsmpl.mode = SIMPLICITI_OFF;

	// Stop acceleration sensor
	as_stop();

	// Powerdown radio
	close_radio();
	
	// Clear last button events
	Timer0_A4_Delay(CONV_MS_TO_TICKS(BUTTONS_DEBOUNCE_TIME_OUT));
	BUTTONS_IFG = 0x00;  
	button.all_flags = 0;
	
	// Clear icons
	display_symbol(LCD_ICON_BEEPER1, SEG_OFF_BLINK_OFF);
	display_symbol(LCD_ICON_BEEPER2, SEG_OFF_BLINK_OFF);
	display_symbol(LCD_ICON_BEEPER3, SEG_OFF_BLINK_OFF);
	
 	// Clean up line 1
	clear_line(LINE1);  	
	display_time(LINE1, DISPLAY_LINE_CLEAR);
	
	// Force full display update
	display.flag.full_update = 1;	
	
}

// *************************************************************************************************
// @fn          display_phase_clock
// @brief       SimpliciTI display routine. 
// @param       u8 line			LINE2
//				u8 update		DISPLAY_LINE_UPDATE_FULL
// @return      none
// *************************************************************************************************
void display_phase_clock(u8 line, u8 update)
{
	if (update == DISPLAY_LINE_UPDATE_FULL)	
	{
		display_chars(LCD_SEG_L2_5_0, (u8 *)" SLEEP", SEG_ON);
	}
}
// *************************************************************************************************
// @fn          is_rf
// @brief       Returns TRUE if SimpliciTI receiver is connected. 
// @param       none
// @return      u8
// *************************************************************************************************
u8 is_sleep(void)
{
	return (sPhase.mode != SLEEP_OFF);
}

#endif /*CONFIG_PHASE_CLOCK*/