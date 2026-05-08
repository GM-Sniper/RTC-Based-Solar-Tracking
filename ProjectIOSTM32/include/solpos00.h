#ifndef SOLPOS00_H
#define SOLPOS00_H

/*============================================================================
*
*     Define the function codes
*
*----------------------------------------------------------------------------*/
#define L_DOY    0x0001
#define L_GEOM   0x0002
#define L_ZENETR 0x0004
#define L_SSHA   0x0008
#define L_SBCF   0x0010
#define L_TST    0x0020
#define L_SRSS   0x0040
#define L_SOLAZM 0x0080
#define L_REFRAC 0x0100
#define L_AMASS  0x0200
#define L_PRIME  0x0400
#define L_TILT   0x0800
#define L_ETR    0x1000
#define L_ALL    0xFFFF

/*============================================================================
*
*     Define the bit-wise masks for each function
*
*----------------------------------------------------------------------------*/
#define S_DOY    ( L_DOY                          )
#define S_GEOM   ( L_GEOM   | S_DOY               )
#define S_ZENETR ( L_ZENETR | S_GEOM              )
#define S_SSHA   ( L_SSHA   | S_GEOM              )
#define S_SBCF   ( L_SBCF   | S_SSHA              )
#define S_TST    ( L_TST    | S_GEOM              )
#define S_SRSS   ( L_SRSS   | S_SSHA   | S_TST    )
#define S_SOLAZM ( L_SOLAZM | S_ZENETR            )
#define S_REFRAC ( L_REFRAC | S_ZENETR            )
#define S_AMASS  ( L_AMASS  | S_REFRAC            )
#define S_PRIME  ( L_PRIME  | S_AMASS             )
#define S_TILT   ( L_TILT   | S_SOLAZM | S_REFRAC )
#define S_ETR    ( L_ETR    | S_REFRAC            )
#define S_ALL    ( L_ALL                          )


/*============================================================================
*
*     Enumerate the error codes
*     (Bit positions are from least significant to most significant)
*
*----------------------------------------------------------------------------*/
enum {S_YEAR_ERROR,    /*  0   year                  1950 -  2050   */
      S_MONTH_ERROR,   /*  1   month                    1 -    12   */
      S_DAY_ERROR,     /*  2   day-of-month             1 -    31   */
      S_DOY_ERROR,     /*  3   day-of-year              1 -   366   */
      S_HOUR_ERROR,    /*  4   hour                     0 -    24   */
      S_MINUTE_ERROR,  /*  5   minute                   0 -    59   */
      S_SECOND_ERROR,  /*  6   second                   0 -    59   */
      S_TZONE_ERROR,   /*  7   time zone              -12 -    12   */
      S_INTRVL_ERROR,  /*  8   interval (seconds)       0 - 28800   */
      S_LAT_ERROR,     /*  9   latitude               -90 -    90   */
      S_LON_ERROR,     /* 10   longitude             -180 -   180   */
      S_TEMP_ERROR,    /* 11   temperature (deg. C)  -100 -   100   */
      S_PRESS_ERROR,   /* 12   pressure (millibars)     0 -  2000   */
      S_TILT_ERROR,    /* 13   tilt                   -90 -    90   */
      S_ASPECT_ERROR,  /* 14   aspect                -360 -   360   */
      S_SBWID_ERROR,   /* 15   shadow band width (cm)   1 -   100   */
      S_SBRAD_ERROR,   /* 16   shadow band radius (cm)  1 -   100   */
      S_SBSKY_ERROR};  /* 17   shadow band sky factor  -1 -     1   */

struct posdata
{
  int   day;
  int   daynum;
  int   function;
  int   hour;
  int   interval;
  int   minute;
  int   month;
  int   second;
  int   year;

  float amass;
  float ampress;
  float aspect;
  float azim;
  float cosinc;
  float coszen;
  float dayang;
  float declin;
  float eclong;
  float ecobli;
  float ectime;
  float elevetr;
  float elevref;
  float eqntim;
  float erv;
  float etr;
  float etrn;
  float etrtilt;
  float gmst;
  float hrang;
  float julday;
  float latitude;
  float longitude;
  float lmst;
  float mnanom;
  float mnlong;
  float rascen;
  float press;
  float prime;
  float sbcf;
  float sbwid;
  float sbrad;
  float sbsky;
  float solcon;
  float ssha;
  float sretr;
  float ssetr;
  float temp;
  float tilt;
  float timezone;
  float tst;
  float tstfix;
  float unprime;
  float utime;
  float zenetr;
  float zenref;
};

long S_solpos (struct posdata *pdat);
void S_init(struct posdata *pdat);
void S_decode(long code, struct posdata *pdat);

#endif // SOLPOS00_H
