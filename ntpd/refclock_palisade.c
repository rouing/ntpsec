/*
 * This software was developed by the Software and Component Technologies
 * group of Trimble Navigation, Ltd.
 *
 * Copyright (c) 1997, 1998, 1999, 2000  Trimble Navigation Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Trimble Navigation, Ltd.
 * 4. The name of Trimble Navigation Ltd. may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TRIMBLE NAVIGATION LTD. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TRIMBLE NAVIGATION LTD. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * refclock_palisade - clock driver for the Trimble Palisade GPS
 * timing receiver
 *
 * For detailed information on this program, please refer to the html 
 * Refclock 29 page accompanying the NTP distribution.
 *
 * for questions / bugs / comments, contact:
 * sven_dietrich@trimble.com
 *
 * Sven-Thorsten Dietrich
 * 645 North Mary Avenue
 * Post Office Box 3642
 * Sunnyvale, CA 94088-3642
 *
 * Version 2.45; July 14, 1999
 *
 *
 *
 * 31/03/06: Added support for Thunderbolt GPS Disciplined Clock.
 *	     Contact: Fernando Pablo Hauscarriaga
 * 	     E-mail: fernandoph@iar.unlp.edu.ar
 * 	     Home page: www.iar.unlp.edu.ar/~fernandoph
 *	          Instituto Argentino de Radioastronomia
 *	                    www.iar.unlp.edu.ar
 *
 * 14/01/07: Conditinal compilation for Thunderbolt support no longer needed
 *           now we use mode 2 for decode thunderbolt packets.
 *           Fernando P. Hauscarriaga
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(SYS_WINNT)
# undef close
# define close closesocket
#endif

#if defined(REFCLOCK) && defined(CLOCK_PALISADE)

#include "refclock_palisade.h"
/* Table to get from month to day of the year */
const int days_of_year [12] = {
	0,  31,  59,  90, 120, 151, 181, 212, 243, 273, 304, 334
};

#ifdef DEBUG
const char * Tracking_Status[15][15] = { 
	{ "Doing Fixes\0" }, { "Good 1SV\0" }, { "Approx. 1SV\0" },
	{"Need Time\0" }, { "Need INIT\0" }, { "PDOP too High\0" },
	{ "Bad 1SV\0" }, { "0SV Usable\0" }, { "1SV Usable\0" },
	{ "2SV Usable\0" }, { "3SV Usable\0" }, { "No Integrity\0" },
	{ "Diff Corr\0" }, { "Overdet Clock\0" }, { "Invalid\0" } };
#endif

/*
 * Transfer vector
 */
struct refclock refclock_palisade = {
	palisade_start,		/* start up driver */
	palisade_shutdown,	/* shut down driver */
	palisade_poll,		/* transmit poll message */
	noentry,		/* not used  */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used */
	NOFLAGS			/* not used */
};

int day_of_year (char *dt);

/* Extract the clock type from the mode setting */
#define CLK_TYPE(x) ((int)(((x)->ttl) & 0x7F))

/* Supported clock types */
#define CLK_TRIMBLE	0	/* Trimble Palisade */
#define CLK_PRAECIS	1	/* Endrun Technologies Praecis */
#define CLK_THUNDERBOLT	2	/* Trimble Thunderbolt GPS Receiver */

int praecis_msg;
static void praecis_parse(struct recvbuf *rbufp, struct peer *peer);

/* These routines are for sending packets to the Thunderbolt receiver
 * They are taken from Markus Prosch
 */

/*
 * sendcmd - Build data packet for sending
 */
static void 
sendcmd (
	struct packettx *buffer,
	int c
	)
{
        buffer->data[0] = DLE;
        buffer->data[1] = (unsigned char)c;
        buffer->size = 2;
}

/*
 * sendsupercmd - Build super data packet for sending
 */
static void 
sendsupercmd (
	struct packettx *buffer,
	int c1,
	int c2
	)
{
        buffer->data[0] = DLE;
        buffer->data[1] = (unsigned char)c1;
        buffer->data[2] = (unsigned char)c2;
        buffer->size = 3;
}

/*
 * sendbyte -
 */
static void 
sendbyte (
	struct packettx *buffer,
	int b
	)
{
        if (b == DLE)
                buffer->data[buffer->size++] = DLE;
        buffer->data[buffer->size++] = (unsigned char)b;
}

/*
 * sendint -
 */
static void 
sendint (
	struct packettx *buffer,
	int a
	)
{
        sendbyte(buffer, (unsigned char)((a>>8) & 0xff));
        sendbyte(buffer, (unsigned char)(a & 0xff));
}

/*
 * sendetx - Send packet or super packet to the device
 */
static int 
sendetx (
	struct packettx *buffer,
	int fd
	)
{
        int result;
	
        buffer->data[buffer->size++] = DLE;
        buffer->data[buffer->size++] = ETX;
        result = write(fd, buffer->data, (unsigned long)buffer->size);
	
        if (result != -1)
                return(result);
        else
                return (-1);
}

/*
 * init_thunderbolt - Prepares Thunderbolt receiver to be used with
 *                    NTP (also taken from Markus Prosch).
 */
static void
init_thunderbolt (
	int fd
	)
{
	u_char buffer[256];
	struct packettx buf;
	
	buf.data = buffer;
	
	/* set UTC time */
	sendsupercmd (&buf, 0x8E, 0xA2);
	sendbyte     (&buf, 0x3);
	sendetx      (&buf, fd);
	
	/* activate packets 0x8F-AB and 0x8F-AC */
	sendsupercmd (&buf, 0x8F, 0xA5);
	sendint      (&buf, 0x5);
	sendetx      (&buf, fd);
}

/*
 * palisade_start - open the devices and initialize data for processing
 */
static int
palisade_start (
	int unit,
	struct peer *peer
	)
{
	struct palisade_unit *up;
	struct refclockproc *pp;
	int fd;
	char gpsdev[20];

	struct termios tio;
#ifdef SYS_WINNT
	(void) sprintf(gpsdev, "COM%d:", unit);
#else	
	(void) sprintf(gpsdev, DEVICE, unit);
#endif
	/*
	 * Open serial port. 
	 */
	fd = refclock_open(gpsdev, SPEED232, LDISC_RAW);
	if (fd <= 0) {
#ifdef DEBUG
		printf("Palisade(%d) start: open %s failed\n", unit, gpsdev);
#endif
		return 0;
	}

	msyslog(LOG_NOTICE, "Palisade(%d) fd: %d dev: %s", unit, fd,
		gpsdev);

        if (tcgetattr(fd, &tio) < 0) {
                msyslog(LOG_ERR, 
			"Palisade(%d) tcgetattr(fd, &tio): %m",unit);
#ifdef DEBUG
                printf("Palisade(%d) tcgetattr(fd, &tio)\n",unit);
#endif
                return (0);
        }

        tio.c_cflag |= (PARENB|PARODD);
        tio.c_iflag &= ~ICRNL;

	/*
	 * Allocate and initialize unit structure
	 */
	up = (struct palisade_unit *) emalloc(sizeof(struct palisade_unit));
	      
	if (!(up)) {
                msyslog(LOG_ERR, "Palisade(%d) emalloc: %m",unit);
#ifdef DEBUG
                printf("Palisade(%d) emalloc\n",unit);
#endif
		(void) close(fd);
		return (0);
	}

	memset((char *)up, 0, sizeof(struct palisade_unit));

	up->type = CLK_TYPE(peer);
	switch (up->type) {
	    case CLK_TRIMBLE:
		/* Normal mode, do nothing */
		break;
	    case CLK_PRAECIS:
		msyslog(LOG_NOTICE, "Palisade(%d) Praecis mode enabled\n"
			,unit);
		break;
	    case CLK_THUNDERBOLT:
		msyslog(LOG_NOTICE, "Palisade(%d) Thunderbolt mode enabled\n"
			,unit);
		tio.c_cflag = (CS8|CLOCAL|CREAD);
		break;		
	    default:
		msyslog(LOG_NOTICE, "Palisade(%d) mode unknown\n",unit);
		break;
	}
	if (tcsetattr(fd, TCSANOW, &tio) == -1) {
                msyslog(LOG_ERR, "Palisade(%d) tcsetattr(fd, &tio): %m",unit);
#ifdef DEBUG
                printf("Palisade(%d) tcsetattr(fd, &tio)\n",unit);
#endif
                return 0;
        }

	pp = peer->procptr;
	pp->io.clock_recv = palisade_io;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
#ifdef DEBUG
                printf("Palisade(%d) io_addclock\n",unit);
#endif
		(void) close(fd);
		free(up);
		return (0);
	}

	/*
	 * Initialize miscellaneous variables
	 */
	pp->unitptr = (caddr_t)up;
	pp->clockdesc = DESCRIPTION;

	peer->precision = PRECISION;
	peer->sstclktype = CTL_SST_TS_UHF;
	peer->minpoll = TRMB_MINPOLL;
	peer->maxpoll = TRMB_MAXPOLL;
	memcpy((char *)&pp->refid, REFID, 4);
	
	up->leap_status = 0;
	up->unit = (short) unit;
	up->rpt_status = TSIP_PARSED_EMPTY;
    	up->rpt_cnt = 0;

    	if (up->type == CLK_THUNDERBOLT)
       	    	init_thunderbolt(fd);

	return 1;
}


/*
 * palisade_shutdown - shut down the clock
 */
static void
palisade_shutdown (
	int unit,
	struct peer *peer
	)
{
	struct palisade_unit *up;
	struct refclockproc *pp;
	pp = peer->procptr;
	up = (struct palisade_unit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}



/* 
 * unpack_date - get day and year from date
 */
int
day_of_year (
	char * dt
	)
{
	int day, mon, year;

	mon = dt[1];
	/* Check month is inside array bounds */
	if ((mon < 1) || (mon > 12)) 
		return -1;

	day = dt[0] + days_of_year[mon - 1];
	year = getint((u_char *) (dt + 2)); 

	if ( !(year % 4) && ((year % 100) || 
			     (!(year % 100) && !(year%400)))
	     &&(mon > 2))
		day ++; /* leap year and March or later */

	return day;
}


/* 
 * TSIP_decode - decode the TSIP data packets 
 */
int
TSIP_decode (
	struct peer *peer
	)
{
	int st;
	long   secint;
	double secs;
	double secfrac;
	unsigned short event = 0;

	struct palisade_unit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct palisade_unit *)pp->unitptr;

	/*
	 * Check the time packet, decode its contents. 
	 * If the timecode has invalid length or is not in
	 * proper format, declare bad format and exit.
	 */

	if (up->type != CLK_THUNDERBOLT){
		if ((up->rpt_buf[0] == (char) 0x41) ||
		    (up->rpt_buf[0] == (char) 0x46) ||
		    (up->rpt_buf[0] == (char) 0x54) ||
		    (up->rpt_buf[0] == (char) 0x4B) ||
		    (up->rpt_buf[0] == (char) 0x6D)) {

			/* standard time packet - GPS time and GPS week number */
#ifdef DEBUG
			printf("Palisade Port B packets detected. Connect to Port A\n");
#endif

			return 0;	
		}
	}

	/*
	 * We cast both to u_char to as 0x8f uses the sign bit on a char
	 */
	if ((u_char) up->rpt_buf[0] == (u_char) 0x8f) {
		/* 
		 * Superpackets
		 */
		event = (unsigned short) (getint((u_char *) &mb(1)) & 0xffff);
		if (!((pp->sloppyclockflag & CLK_FLAG2) || event)) 
			/* Ignore Packet */
			return 0;	   
	
		switch (mb(0) & 0xff) {
			int GPS_UTC_Offset;
			long tow;

		    case PACKET_8F0B: 

			if (up->polled <= 0)
				return 0;

			if (up->rpt_cnt != LENCODE_8F0B)  /* check length */
				break;
		
#ifdef DEBUG
			if (debug > 1) {
				int ts;
				double lat, lon, alt;
				lat = getdbl((u_char *) &mb(42)) * R2D;
				lon = getdbl((u_char *) &mb(50)) * R2D;
				alt = getdbl((u_char *) &mb(58));

				printf("TSIP_decode: unit %d: Latitude: %03.4f Longitude: %03.4f Alt: %05.2f m\n",
				       up->unit, lat,lon,alt);
				printf("TSIP_decode: unit %d: Sats:",
				       up->unit);
				for (st = 66, ts = 0; st <= 73; st++)
					if (mb(st)) {
						if (mb(st) > 0) ts++;
						printf(" %02d", mb(st));
					}
				printf(" : Tracking %d\n", ts); 
			}
#endif

			GPS_UTC_Offset = getint((u_char *) &mb(16));  
			if (GPS_UTC_Offset == 0) { /* Check UTC offset */ 
#ifdef DEBUG
				printf("TSIP_decode: UTC Offset Unknown\n");
#endif
				break;
			}

			secs = getdbl((u_char *) &mb(3));
			secint = (long) secs;
			secfrac = secs - secint; /* 0.0 <= secfrac < 1.0 */

			pp->nsec = (long) (secfrac * 1000000000); 

			secint %= 86400;    /* Only care about today */
			pp->hour = secint / 3600;
			secint %= 3600;
			pp->minute = secint / 60;
			secint %= 60;
			pp->second = secint % 60;
		
			if ((pp->day = day_of_year(&mb(11))) < 0) break;

			pp->year = getint((u_char *) &mb(13)); 

#ifdef DEBUG
			if (debug > 1)
				printf("TSIP_decode: unit %d: %02X #%d %02d:%02d:%02d.%06ld %02d/%02d/%04d UTC %02d\n",
				       up->unit, mb(0) & 0xff, event, pp->hour, pp->minute, 
				       pp->second, pp->nsec, mb(12), mb(11), pp->year, GPS_UTC_Offset);
#endif
			/* Only use this packet when no
			 * 8F-AD's are being received
			 */

			if (up->leap_status) {
				up->leap_status = 0;
				return 0;
			}

			return 2;
			break;

		    case PACKET_NTP:
			/* Palisade-NTP Packet */

			if (up->rpt_cnt != LENCODE_NTP) /* check length */
				break;
	
			up->leap_status = mb(19);

			if (up->polled  <= 0) 
				return 0;
				
			/* Check Tracking Status */
			st = mb(18);
			if (st < 0 || st > 14)
				st = 14;
			if ((st >= 2 && st <= 7) || st == 11 || st == 12) {
#ifdef DEBUG
				printf("TSIP_decode: Not Tracking Sats : %s\n",
				       *Tracking_Status[st]);
#endif
				refclock_report(peer, CEVNT_BADTIME);
				up->polled = -1;
				return 0;
				break;
			}

			if (up->leap_status & PALISADE_LEAP_PENDING) {
				if (up->leap_status & PALISADE_UTC_TIME)  
					pp->leap = LEAP_ADDSECOND;
				else
					pp->leap = LEAP_DELSECOND;
			}
			else if (up->leap_status)
				pp->leap = LEAP_NOWARNING;
		
			else {  /* UTC flag is not set:
				 * Receiver may have been reset, and lost
				 * its UTC almanac data */
				pp->leap = LEAP_NOTINSYNC;
#ifdef DEBUG
				printf("TSIP_decode: UTC Almanac unavailable: %d\n",
				       mb(19));	
#endif
				refclock_report(peer, CEVNT_BADTIME);
				up->polled = -1;
				return 0;
			}

			pp->nsec = (long) (getdbl((u_char *) &mb(3))
					   * 1000000000);

			if ((pp->day = day_of_year(&mb(14))) < 0) 
				break;
			pp->year = getint((u_char *) &mb(16)); 
			pp->hour = mb(11);
			pp->minute = mb(12);
			pp->second = mb(13);

#ifdef DEBUG
			if (debug > 1)
				printf("TSIP_decode: unit %d: %02X #%d %02d:%02d:%02d.%06ld %02d/%02d/%04d UTC %02x %s\n",
				       up->unit, mb(0) & 0xff, event, pp->hour, pp->minute, 
				       pp->second, pp->nsec, mb(15), mb(14), pp->year,
				       mb(19), *Tracking_Status[st]);
#endif
			return 1;
			break;

		    case PACKET_8FAC:   
			if (up->polled <= 0)
				return 0; 

			if (up->rpt_cnt != LENCODE_8FAC)/* check length */
				break;

#ifdef DEBUG
			if (debug > 1) {
				double lat, lon, alt;
				lat = getdbl((u_char *) &mb(36)) * R2D;
				lon = getdbl((u_char *) &mb(44)) * R2D;
				alt = getdbl((u_char *) &mb(52));

				printf("TSIP_decode: unit %d: Latitude: %03.4f Longitude: %03.4f Alt: %05.2f m\n",
				       up->unit, lat,lon,alt);
				printf("TSIP_decode: unit %d\n", up->unit);
			}
#endif
			if (getint((u_char *) &mb(10)) & 0x80) 
				pp->leap = LEAP_ADDSECOND;  /* we ASSUME addsecond */
			else 
				pp->leap = LEAP_NOWARNING;
                		
#ifdef DEBUG
			if (debug > 1) 
				printf("TSIP_decode: unit %d: 0x%02x leap %d\n",
				       up->unit, mb(0) & 0xff, pp->leap);
			if (debug > 1) {
				printf("Receiver MODE: 0x%02X\n", (u_char)mb(1));
				if (mb(1) == 0x00)
					printf("                AUTOMATIC\n");
				if (mb(1) == 0x01)
					printf("                SINGLE SATELLITE\n");   
				if (mb(1) == 0x03)
					printf("                HORIZONTAL(2D)\n");
				if (mb(1) == 0x04)
					printf("                FULL POSITION(3D)\n");
				if (mb(1) == 0x05)
					printf("                DGPR REFERENCE\n");
				if (mb(1) == 0x06)
					printf("                CLOCK HOLD(2D)\n");
				if (mb(1) == 0x07)
					printf("                OVERDETERMINED CLOCK\n");

				printf("\n** Disciplining MODE 0x%02X:\n", (u_char)mb(2));
				if (mb(2) == 0x00)
					printf("                NORMAL\n");
				if (mb(2) == 0x01)
					printf("                POWER-UP\n");
				if (mb(2) == 0x02)
					printf("                AUTO HOLDOVER\n");
				if (mb(2) == 0x03)
					printf("                MANUAL HOLDOVER\n");
				if (mb(2) == 0x04)
					printf("                RECOVERY\n");
				if (mb(2) == 0x06)
					printf("                DISCIPLINING DISABLED\n");
			}
#endif   
			return 0;
			break;

		    case PACKET_8FAB:
			/* Thunderbolt Primary Timing Packet */
	        
			if (up->rpt_cnt != LENCODE_8FAB) /* check length */
				break;

			if (up->polled  <= 0)
				return 0;

			GPS_UTC_Offset = getint((u_char *) &mb(7));
                
			if (GPS_UTC_Offset == 0){ /* Check UTC Offset */
#ifdef DEBUG
				printf("TSIP_decode: UTC Offset Unknown\n");
#endif
				break;
			}


			if ((mb(9) & 0x1d) == 0x0) {
				/* if we know the GPS time and the UTC offset,
				   we expect UTC timing information !!! */

				pp->leap = LEAP_NOTINSYNC;
				refclock_report(peer, CEVNT_BADTIME);
				up->polled = -1;
				return 0;
			}

			pp->nsec = 0;
#ifdef DEBUG		
			printf("\nTiming Flags are:\n");
			printf("Timing flag value is: 0x%X\n", mb(9));
			if ((mb(9) & 0x01) != 0)
				printf ("	Getting UTC time\n");
			else
				printf ("	Getting GPS time\n");
			if ((mb(9) & 0x02) != 0)
				printf ("	PPS is from UTC\n");
			else
				printf ("	PPS is from GPS\n");
			if ((mb(9) & 0x04) != 0)
				printf ("	Time is not Set\n");
			else
				printf ("	Time is Set\n");
			if ((mb(9) & 0x08) != 0)
				printf("	I dont have UTC info\n");
			else
				printf ("	I have UTC info\n");
			if ((mb(9) & 0x10) != 0)
				printf ("	Time is from USER\n\n");
			else
				printf ("	Time is from GPS\n\n");	
#endif		

			if ((pp->day = day_of_year(&mb(13))) < 0)
				break;
			tow = getlong((u_char *) &mb(1));
#ifdef DEBUG		
			if (debug > 1) {
				printf("pp->day: %d\n", pp->day); 
				printf("TOW: %ld\n", tow);
				printf("DAY: %d\n", mb(13));
			}
#endif
			pp->year = getint((u_char *) &mb(15));
			pp->hour = mb(12);
			pp->minute = mb(11);
			pp->second = mb(10);


#ifdef DEBUG
			if (debug > 1)
				printf("TSIP_decode: unit %d: %02X #%d %02d:%02d:%02d.%06ld %02d/%02d/%04d ",up->unit, mb(0) & 0xff, event, pp->hour, pp->minute, pp->second, pp->nsec, mb(14), mb(13), pp->year);
#endif
			return 1;
			break;

		    default: 	
			/* Ignore Packet */
			return 0;
		} /* switch */
	}/* if 8F packets */	

        else if (up->rpt_buf[0] == (u_char)0x42) {
		printf("0x42\n");
		return 0;
        }
        else if (up->rpt_buf[0] == (u_char)0x43) {
		printf("0x43\n");
		return 0;
        }
        else if (up->rpt_buf[0] == PACKET_41) {
		printf("0x41\n");
		return 0;
        }
        else if (up->rpt_buf[0] == PACKET_6D) {
#ifdef DEBUG
		int sats;

		if ((mb(0) & 0x01) && (mb(0) & 0x02))
			printf("2d Fix Dimension\n");
		if (mb(0) & 0x04)
			printf("3d Fix Dimension\n");

		if (mb(0) & 0x08)
			printf("Fix Mode is MANUAL\n");
		else
			printf("Fix Mode is AUTO\n");
	
		sats = mb(0) & 0xF0;
		sats = sats >> 4;
		printf("Tracking %d Satellites\n", sats);
#endif
		return 0;
        } /* else if not super packet */
	refclock_report(peer, CEVNT_BADREPLY);
	up->polled = -1;
#ifdef DEBUG
	printf("TSIP_decode: unit %d: bad packet %02x-%02x event %d len %d\n", 
	       up->unit, up->rpt_buf[0] & 0xff, mb(0) & 0xff, 
	       event, up->rpt_cnt);
#endif
	return 0;
}

/*
 * palisade__receive - receive data from the serial interface
 */

static void
palisade_receive (
	struct peer * peer
	)
{
	struct palisade_unit *up;
	struct refclockproc *pp;

	/*
	 * Initialize pointers and read the timecode and timestamp.
	 */
	pp = peer->procptr;
	up = (struct palisade_unit *)pp->unitptr;
		
	if (! TSIP_decode(peer)) return;
	
	if (up->polled <= 0) 
		return;   /* no poll pending, already received or timeout */

	up->polled = 0;  /* Poll reply received */
	pp->lencode = 0; /* clear time code */
#ifdef DEBUG
	if (debug) 
		printf(
			"palisade_receive: unit %d: %4d %03d %02d:%02d:%02d.%06ld\n",
			up->unit, pp->year, pp->day, pp->hour, pp->minute, 
			pp->second, pp->nsec);
#endif

	/*
	 * Process the sample
	 * Generate timecode: YYYY DoY HH:MM:SS.microsec 
	 * report and process 
	 */

	(void) sprintf(pp->a_lastcode,"%4d %03d %02d:%02d:%02d.%06ld",
		       pp->year,pp->day,pp->hour,pp->minute, pp->second,pp->nsec); 
	pp->lencode = 24;

	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);

#ifdef DEBUG
		printf("palisade_receive: unit %d: refclock_process failed!\n",
		       up->unit);
#endif
		return;
	}

	record_clock_stats(&peer->srcadr, pp->a_lastcode); 

#ifdef DEBUG
	if (debug)
		printf("palisade_receive: unit %d: %s\n",
		       up->unit, prettydate(&pp->lastrec));
#endif
	pp->lastref = pp->lastrec;
	refclock_receive(peer);
}


/*
 * palisade_poll - called by the transmit procedure
 *
 */
static void
palisade_poll (
	int unit,
	struct peer *peer
	)
{
	struct palisade_unit *up;
	struct refclockproc *pp;
	
	pp = peer->procptr;
	up = (struct palisade_unit *)pp->unitptr;

	pp->polls++;
	if (up->polled > 0) /* last reply never arrived or error */ 
		refclock_report(peer, CEVNT_TIMEOUT);

	up->polled = 2; /* synchronous packet + 1 event */
	
#ifdef DEBUG
	if (debug)
		printf("palisade_poll: unit %d: polling %s\n", unit,
		       (pp->sloppyclockflag & CLK_FLAG2) ? 
		       "synchronous packet" : "event");
#endif 

	if (pp->sloppyclockflag & CLK_FLAG2) 
		return;  /* using synchronous packet input */

	if(up->type == CLK_PRAECIS) {
		if(write(peer->procptr->io.fd,"SPSTAT\r\n",8) < 0)
			msyslog(LOG_ERR, "Palisade(%d) write: %m:",unit);
		else {
			praecis_msg = 1;
			return;
		}
	}

	if (HW_poll(pp) < 0) 
		refclock_report(peer, CEVNT_FAULT); 
}

static void
praecis_parse (
	struct recvbuf *rbufp,
	struct peer *peer
	)
{
	static char buf[100];
	static int p = 0;
	struct refclockproc *pp;

	pp = peer->procptr;

	memcpy(buf+p,rbufp->recv_space.X_recv_buffer, rbufp->recv_length);
	p += rbufp->recv_length;

	if(buf[p-2] == '\r' && buf[p-1] == '\n') {
		buf[p-2] = '\0';
		record_clock_stats(&peer->srcadr, buf);

		p = 0;
		praecis_msg = 0;

		if (HW_poll(pp) < 0)
			refclock_report(peer, CEVNT_FAULT);

	}
}

static void
palisade_io (
	struct recvbuf *rbufp
	)
{
	/*
	 * Initialize pointers and read the timecode and timestamp.
	 */
	struct palisade_unit *up;
	struct refclockproc *pp;
	struct peer *peer;

	char * c, * d;

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct palisade_unit *)pp->unitptr;

	if(up->type == CLK_PRAECIS) {
		if(praecis_msg) {
			praecis_parse(rbufp,peer);
			return;
		}
	}

	c = (char *) &rbufp->recv_space;
	d = c + rbufp->recv_length;
		
	while (c != d) {

		/* Build time packet */
		switch (up->rpt_status) {

		    case TSIP_PARSED_DLE_1:
			switch (*c)
			{
			    case 0:
			    case DLE:
			    case ETX:
				up->rpt_status = TSIP_PARSED_EMPTY;
				break;

			    default:
				up->rpt_status = TSIP_PARSED_DATA;
				/* save packet ID */
				up->rpt_buf[0] = *c;
				break;
			}
			break;

		    case TSIP_PARSED_DATA:
			if (*c == DLE)
				up->rpt_status = TSIP_PARSED_DLE_2;
			else 
				mb(up->rpt_cnt++) = *c;
			break;

		    case TSIP_PARSED_DLE_2:
			if (*c == DLE) {
				up->rpt_status = TSIP_PARSED_DATA;
				mb(up->rpt_cnt++) = 
				    *c;
			}       
			else if (*c == ETX) 
				up->rpt_status = TSIP_PARSED_FULL;
			else 	{
                        	/* error: start new report packet */
				up->rpt_status = TSIP_PARSED_DLE_1;
				up->rpt_buf[0] = *c;
			}
			break;

		    case TSIP_PARSED_FULL:
		    case TSIP_PARSED_EMPTY:
		    default:
		        if ( *c != DLE)
				up->rpt_status = TSIP_PARSED_EMPTY;
			else 
				up->rpt_status = TSIP_PARSED_DLE_1;
                        break;
		}
		
		c++;

		if (up->rpt_status == TSIP_PARSED_DLE_1) {
			up->rpt_cnt = 0;
			if (pp->sloppyclockflag & CLK_FLAG2) 
                		/* stamp it */
				get_systime(&pp->lastrec);
		}
		else if (up->rpt_status == TSIP_PARSED_EMPTY)
		    	up->rpt_cnt = 0;

		else if (up->rpt_cnt > BMAX) 
			up->rpt_status =TSIP_PARSED_EMPTY;

		if (up->rpt_status == TSIP_PARSED_FULL) 
			palisade_receive(peer);

	} /* while chars in buffer */
}


/*
 * Trigger the Palisade's event input, which is driven off the RTS
 *
 * Take a system time stamp to match the GPS time stamp.
 *
 */
long
HW_poll (
	struct refclockproc * pp 	/* pointer to unit structure */
	)
{	
	int x;	/* state before & after RTS set */
	struct palisade_unit *up;

	up = (struct palisade_unit *) pp->unitptr;

	/* read the current status, so we put things back right */
	if (ioctl(pp->io.fd, TIOCMGET, &x) < 0) {
#ifdef DEBUG
		if (debug)
			printf("Palisade HW_poll: unit %d: GET %s\n", up->unit, strerror(errno));
#endif
		msyslog(LOG_ERR, "Palisade(%d) HW_poll: ioctl(fd,GET): %m", 
			up->unit);
		return -1;
	}
  
	x |= TIOCM_RTS;        /* turn on RTS  */

	/* Edge trigger */
	if (ioctl(pp->io.fd, TIOCMSET, &x) < 0) { 
#ifdef DEBUG
		if (debug)
			printf("Palisade HW_poll: unit %d: SET \n", up->unit);
#endif
		msyslog(LOG_ERR,
			"Palisade(%d) HW_poll: ioctl(fd, SET, RTS_on): %m", 
			up->unit);
		return -1;
	}

	x &= ~TIOCM_RTS;        /* turn off RTS  */
	
	/* poll timestamp */
	get_systime(&pp->lastrec);

	if (ioctl(pp->io.fd, TIOCMSET, &x) == -1) {
#ifdef DEBUG
		if (debug)
			printf("Palisade HW_poll: unit %d: UNSET \n", up->unit);
#endif
		msyslog(LOG_ERR,
			"Palisade(%d) HW_poll: ioctl(fd, UNSET, RTS_off): %m", 
			up->unit);
		return -1;
	}

	return 0;
}

#if 0 /* unused */
/*
 * this 'casts' a character array into a float
 */
float
getfloat (
	u_char *bp
	)
{
	float sval;
#ifdef WORDS_BIGENDIAN 
	((char *) &sval)[0] = *bp++;
	((char *) &sval)[1] = *bp++;
	((char *) &sval)[2] = *bp++;
	((char *) &sval)[3] = *bp++;
#else
	((char *) &sval)[3] = *bp++;
	((char *) &sval)[2] = *bp++;
	((char *) &sval)[1] = *bp++;
	((char *) &sval)[0] = *bp;
#endif  /* ! XNTP_BIG_ENDIAN */ 
	return sval;
}
#endif

/*
 * this 'casts' a character array into a double
 */
double
getdbl (
	u_char *bp
	)
{
	double dval;
#ifdef WORDS_BIGENDIAN 
	((char *) &dval)[0] = *bp++;
	((char *) &dval)[1] = *bp++;
	((char *) &dval)[2] = *bp++;
	((char *) &dval)[3] = *bp++;
	((char *) &dval)[4] = *bp++;
	((char *) &dval)[5] = *bp++;
	((char *) &dval)[6] = *bp++;
	((char *) &dval)[7] = *bp;
#else
	((char *) &dval)[7] = *bp++;
	((char *) &dval)[6] = *bp++;
	((char *) &dval)[5] = *bp++;
	((char *) &dval)[4] = *bp++;
	((char *) &dval)[3] = *bp++;
	((char *) &dval)[2] = *bp++;
	((char *) &dval)[1] = *bp++;
	((char *) &dval)[0] = *bp;
#endif  /* ! XNTP_BIG_ENDIAN */ 
	return dval;
}

/*
 * cast a 16 bit character array into a short (16 bit) int
 */
short
getint (
	u_char *bp
	)
{
	return (short) (bp[1] + (bp[0] << 8));
}

/*
 * cast a 32 bit character array into a long (32 bit) int
 */
long
getlong(
	u_char *bp
	)
{
	return (long) (bp[0] << 24) | 
	    (bp[1] << 16) |
	    (bp[2] << 8) |
	    bp[3];
}

int refclock_palisade_bs;
#endif /* REFCLOCK */
