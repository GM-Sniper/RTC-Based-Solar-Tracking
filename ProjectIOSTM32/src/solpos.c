#include <math.h>
#include <string.h>
#include <stdio.h>
#include "solpos00.h"

struct trigdata
{
  float cd;
  float ch;
  float cl;
  float sd;
  float sl;
};

static int month_days[2][13] = {{0, 0, 31, 59, 90, 120, 151,
                                 181, 212, 243, 273, 304, 334},
                                {0, 0, 31, 60, 91, 121, 152,
                                 182, 213, 244, 274, 305, 335}};

static float degrad = 57.295779513;
static float raddeg = 0.0174532925;

static long int validate(struct posdata *pdat);
static void dom2doy(struct posdata *pdat);
static void doy2dom(struct posdata *pdat);
static void geometry(struct posdata *pdat);
static void zen_no_ref(struct posdata *pdat, struct trigdata *tdat);
static void ssha(struct posdata *pdat, struct trigdata *tdat);
static void sbcf(struct posdata *pdat, struct trigdata *tdat);
static void tst(struct posdata *pdat);
static void srss(struct posdata *pdat);
static void sazm(struct posdata *pdat, struct trigdata *tdat);
static void refrac(struct posdata *pdat);
static void amass(struct posdata *pdat);
static void prime(struct posdata *pdat);
static void etr(struct posdata *pdat);
static void tilt(struct posdata *pdat);
static void localtrig(struct posdata *pdat, struct trigdata *tdat);

long S_solpos(struct posdata *pdat)
{
  long int retval;
  struct trigdata trigdat, *tdat;
  tdat = &trigdat;
  tdat->sd = -999.0;
  tdat->cd = 1.0;
  tdat->ch = 1.0;
  tdat->cl = 1.0;
  tdat->sl = 1.0;

  if ((retval = validate(pdat)) != 0)
    return retval;

  if (pdat->function & L_DOY)
    doy2dom(pdat);
  else
    dom2doy(pdat);

  if (pdat->function & L_GEOM)
    geometry(pdat);

  if (pdat->function & L_ZENETR)
    zen_no_ref(pdat, tdat);

  if (pdat->function & L_SSHA)
    ssha(pdat, tdat);

  if (pdat->function & L_SBCF)
    sbcf(pdat, tdat);

  if (pdat->function & L_TST)
    tst(pdat);

  if (pdat->function & L_SRSS)
    srss(pdat);

  if (pdat->function & L_SOLAZM)
    sazm(pdat, tdat);

  if (pdat->function & L_REFRAC)
    refrac(pdat);

  if (pdat->function & L_AMASS)
    amass(pdat);

  if (pdat->function & L_PRIME)
    prime(pdat);

  if (pdat->function & L_ETR)
    etr(pdat);

  if (pdat->function & L_TILT)
    tilt(pdat);

  return 0;
}

void S_init(struct posdata *pdat)
{
  pdat->day = -99;
  pdat->daynum = -999;
  pdat->hour = -99;
  pdat->minute = -99;
  pdat->month = -99;
  pdat->second = -99;
  pdat->year = -99;
  pdat->interval = 0;
  pdat->aspect = 180.0;
  pdat->latitude = -99.0;
  pdat->longitude = -999.0;
  pdat->press = 1013.0;
  pdat->solcon = 1367.0;
  pdat->temp = 15.0;
  pdat->tilt = 0.0;
  pdat->timezone = -99.0;
  pdat->sbwid = 7.6;
  pdat->sbrad = 31.7;
  pdat->sbsky = 0.04;
  pdat->function = S_ALL;
}

static long int validate(struct posdata *pdat)
{
  long int retval = 0;

  if (pdat->function & L_GEOM)
  {
    if ((pdat->year < 1950) || (pdat->year > 2050))
      retval |= (1L << S_YEAR_ERROR);
    if (!(pdat->function & S_DOY) && ((pdat->month < 1) || (pdat->month > 12)))
      retval |= (1L << S_MONTH_ERROR);
    if (!(pdat->function & S_DOY) && ((pdat->day < 1) || (pdat->day > 31)))
      retval |= (1L << S_DAY_ERROR);
    if ((pdat->function & S_DOY) && ((pdat->daynum < 1) || (pdat->daynum > 366)))
      retval |= (1L << S_DOY_ERROR);

    if ((pdat->hour < 0) || (pdat->hour > 24))
      retval |= (1L << S_HOUR_ERROR);
    if ((pdat->minute < 0) || (pdat->minute > 59))
      retval |= (1L << S_MINUTE_ERROR);
    if ((pdat->second < 0) || (pdat->second > 59))
      retval |= (1L << S_SECOND_ERROR);
    if ((pdat->hour == 24) && (pdat->minute > 0))
      retval |= ((1L << S_HOUR_ERROR) | (1L << S_MINUTE_ERROR));
    if ((pdat->hour == 24) && (pdat->second > 0))
      retval |= ((1L << S_HOUR_ERROR) | (1L << S_SECOND_ERROR));
    if (fabs(pdat->timezone) > 12.0)
      retval |= (1L << S_TZONE_ERROR);
    if ((pdat->interval < 0) || (pdat->interval > 28800))
      retval |= (1L << S_INTRVL_ERROR);

    if (fabs(pdat->longitude) > 180.0)
      retval |= (1L << S_LON_ERROR);
    if (fabs(pdat->latitude) > 90.0)
      retval |= (1L << S_LAT_ERROR);
  }

  if ((pdat->function & L_REFRAC) && (fabs(pdat->temp) > 100.0))
    retval |= (1L << S_TEMP_ERROR);
  if ((pdat->function & L_REFRAC) &&
      ((pdat->press < 0.0) || (pdat->press > 2000.0)))
    retval |= (1L << S_PRESS_ERROR);

  if ((pdat->function & L_TILT) && (fabs(pdat->tilt) > 180.0))
    retval |= (1L << S_TILT_ERROR);
  if ((pdat->function & L_TILT) && (fabs(pdat->aspect) > 360.0))
    retval |= (1L << S_ASPECT_ERROR);

  if ((pdat->function & L_SBCF) &&
      ((pdat->sbwid < 1.0) || (pdat->sbwid > 100.0)))
    retval |= (1L << S_SBWID_ERROR);
  if ((pdat->function & L_SBCF) &&
      ((pdat->sbrad < 1.0) || (pdat->sbrad > 100.0)))
    retval |= (1L << S_SBRAD_ERROR);
  if ((pdat->function & L_SBCF) && (fabs(pdat->sbsky) > 1.0))
    retval |= (1L << S_SBSKY_ERROR);

  return retval;
}

static void dom2doy(struct posdata *pdat)
{
  pdat->daynum = pdat->day + month_days[0][pdat->month];

  if (((pdat->year % 4) == 0) &&
      (((pdat->year % 100) != 0) || ((pdat->year % 400) == 0)) &&
      (pdat->month > 2))
    pdat->daynum += 1;
}

static void doy2dom(struct posdata *pdat)
{
  int imon;
  int leap;

  if (((pdat->year % 4) == 0) &&
      (((pdat->year % 100) != 0) || ((pdat->year % 400) == 0)))
    leap = 1;
  else
    leap = 0;

  imon = 12;
  while (pdat->daynum <= month_days[leap][imon])
    --imon;

  pdat->month = imon;
  pdat->day = pdat->daynum - month_days[leap][imon];
}

static void geometry(struct posdata *pdat)
{
  float bottom, c2, cd, d2, delta, s2, sd, top;
  int leap;

  pdat->dayang = 360.0 * (pdat->daynum - 1) / 365.0;

  sd = sin(raddeg * pdat->dayang);
  cd = cos(raddeg * pdat->dayang);
  d2 = 2.0 * pdat->dayang;
  c2 = cos(raddeg * d2);
  s2 = sin(raddeg * d2);

  pdat->erv = 1.000110 + 0.034221 * cd + 0.001280 * sd;
  pdat->erv += 0.000719 * c2 + 0.000077 * s2;

  pdat->utime =
      pdat->hour * 3600.0 +
      pdat->minute * 60.0 +
      pdat->second -
      (float)pdat->interval / 2.0;
  pdat->utime = pdat->utime / 3600.0 - pdat->timezone;

  delta = pdat->year - 1949;
  leap = (int)(delta / 4.0);
  pdat->julday =
      32916.5 + delta * 365.0 + leap + pdat->daynum + pdat->utime / 24.0;

  pdat->ectime = pdat->julday - 51545.0;

  pdat->mnlong = 280.460 + 0.9856474 * pdat->ectime;

  pdat->mnlong -= 360.0 * (int)(pdat->mnlong / 360.0);
  if (pdat->mnlong < 0.0)
    pdat->mnlong += 360.0;

  pdat->mnanom = 357.528 + 0.9856003 * pdat->ectime;

  pdat->mnanom -= 360.0 * (int)(pdat->mnanom / 360.0);
  if (pdat->mnanom < 0.0)
    pdat->mnanom += 360.0;

  pdat->eclong = pdat->mnlong + 1.915 * sin(pdat->mnanom * raddeg) +
                 0.020 * sin(2.0 * pdat->mnanom * raddeg);

  pdat->eclong -= 360.0 * (int)(pdat->eclong / 360.0);
  if (pdat->eclong < 0.0)
    pdat->eclong += 360.0;

  pdat->ecobli = 23.439 - 4.0e-07 * pdat->ectime;

  pdat->declin = degrad * asin(sin(pdat->ecobli * raddeg) *
                               sin(pdat->eclong * raddeg));

  top = cos(raddeg * pdat->ecobli) * sin(raddeg * pdat->eclong);
  bottom = cos(raddeg * pdat->eclong);

  pdat->rascen = degrad * atan2(top, bottom);

  if (pdat->rascen < 0.0)
    pdat->rascen += 360.0;

  pdat->gmst = 6.697375 + 0.0657098242 * pdat->ectime + pdat->utime;

  pdat->gmst -= 24.0 * (int)(pdat->gmst / 24.0);
  if (pdat->gmst < 0.0)
    pdat->gmst += 24.0;

  pdat->lmst = pdat->gmst * 15.0 + pdat->longitude;

  pdat->lmst -= 360.0 * (int)(pdat->lmst / 360.0);
  if (pdat->lmst < 0.)
    pdat->lmst += 360.0;

  pdat->hrang = pdat->lmst - pdat->rascen;

  if (pdat->hrang < -180.0)
    pdat->hrang += 360.0;
  else if (pdat->hrang > 180.0)
    pdat->hrang -= 360.0;
}

static void zen_no_ref(struct posdata *pdat, struct trigdata *tdat)
{
  float cz;

  localtrig(pdat, tdat);
  cz = tdat->sd * tdat->sl + tdat->cd * tdat->cl * tdat->ch;

  if (fabs(cz) > 1.0)
  {
    if (cz >= 0.0)
      cz = 1.0;
    else
      cz = -1.0;
  }

  pdat->zenetr = acos(cz) * degrad;

  if (pdat->zenetr > 99.0)
    pdat->zenetr = 99.0;

  pdat->elevetr = 90.0 - pdat->zenetr;
}

static void ssha(struct posdata *pdat, struct trigdata *tdat)
{
  float cssha, cdcl;

  localtrig(pdat, tdat);
  cdcl = tdat->cd * tdat->cl;

  if (fabs(cdcl) >= 0.001)
  {
    cssha = -tdat->sl * tdat->sd / cdcl;

    if (cssha < -1.0)
      pdat->ssha = 180.0;
    else if (cssha > 1.0)
      pdat->ssha = 0.0;
    else
      pdat->ssha = degrad * acos(cssha);
  }
  else if (((pdat->declin >= 0.0) && (pdat->latitude > 0.0)) ||
           ((pdat->declin < 0.0) && (pdat->latitude < 0.0)))
    pdat->ssha = 180.0;
  else
    pdat->ssha = 0.0;
}

static void sbcf(struct posdata *pdat, struct trigdata *tdat)
{
  float p, t1, t2;

  localtrig(pdat, tdat);
  p = 0.6366198 * pdat->sbwid / pdat->sbrad * pow(tdat->cd, 3);
  t1 = tdat->sl * tdat->sd * pdat->ssha * raddeg;
  t2 = tdat->cl * tdat->cd * sin(pdat->ssha * raddeg);
  pdat->sbcf = pdat->sbsky + 1.0 / (1.0 - p * (t1 + t2));
}

static void tst(struct posdata *pdat)
{
  pdat->tst = (180.0 + pdat->hrang) * 4.0;
  pdat->tstfix =
      pdat->tst -
      (float)pdat->hour * 60.0 -
      pdat->minute -
      (float)pdat->second / 60.0 +
      (float)pdat->interval / 120.0;

  while (pdat->tstfix > 720.0)
    pdat->tstfix -= 1440.0;
  while (pdat->tstfix < -720.0)
    pdat->tstfix += 1440.0;

  pdat->eqntim =
      pdat->tstfix + 60.0 * pdat->timezone - 4.0 * pdat->longitude;
}

static void srss(struct posdata *pdat)
{
  if (pdat->ssha <= 1.0)
  {
    pdat->sretr = 2999.0;
    pdat->ssetr = -2999.0;
  }
  else if (pdat->ssha >= 179.0)
  {
    pdat->sretr = -2999.0;
    pdat->ssetr = 2999.0;
  }
  else
  {
    pdat->sretr = 720.0 - 4.0 * pdat->ssha - pdat->tstfix;
    pdat->ssetr = 720.0 + 4.0 * pdat->ssha - pdat->tstfix;
  }
}

static void sazm(struct posdata *pdat, struct trigdata *tdat)
{
  float ca, ce, cecl, se;

  localtrig(pdat, tdat);
  ce = cos(raddeg * pdat->elevetr);
  se = sin(raddeg * pdat->elevetr);

  pdat->azim = 180.0;
  cecl = ce * tdat->cl;
  if (fabs(cecl) >= 0.001)
  {
    ca = (se * tdat->sl - tdat->sd) / cecl;
    if (ca > 1.0)
      ca = 1.0;
    else if (ca < -1.0)
      ca = -1.0;

    pdat->azim = 180.0 - acos(ca) * degrad;
    if (pdat->hrang > 0)
      pdat->azim = 360.0 - pdat->azim;
  }
}

static void refrac(struct posdata *pdat)
{
  float prestemp, refcor, tanelev;

  if (pdat->elevetr > 85.0)
    refcor = 0.0;
  else
  {
    tanelev = tan(raddeg * pdat->elevetr);
    if (pdat->elevetr >= 5.0)
      refcor = 58.1 / tanelev -
               0.07 / (pow(tanelev, 3)) +
               0.000086 / (pow(tanelev, 5));
    else if (pdat->elevetr >= -0.575)
      refcor = 1735.0 +
               pdat->elevetr * (-518.2 + pdat->elevetr * (103.4 +
                                                          pdat->elevetr * (-12.79 + pdat->elevetr * 0.711)));
    else
      refcor = -20.774 / tanelev;

    prestemp =
        (pdat->press * 283.0) / (1013.0 * (273.0 + pdat->temp));
    refcor *= prestemp / 3600.0;
  }

  pdat->elevref = pdat->elevetr + refcor;

  if (pdat->elevref < -9.0)
    pdat->elevref = -9.0;

  pdat->zenref = 90.0 - pdat->elevref;
  pdat->coszen = cos(raddeg * pdat->zenref);
}

static void amass(struct posdata *pdat)
{
  if (pdat->zenref > 93.0)
  {
    pdat->amass = -1.0;
    pdat->ampress = -1.0;
  }
  else
  {
    pdat->amass =
        1.0 / (cos(raddeg * pdat->zenref) + 0.50572 *
                                                pow((96.07995 - pdat->zenref), -1.6364));

    pdat->ampress = pdat->amass * pdat->press / 1013.0;
  }
}

static void prime(struct posdata *pdat)
{
  pdat->unprime = 1.031 * exp(-1.4 / (0.9 + 9.4 / pdat->amass)) + 0.1;
  pdat->prime = 1.0 / pdat->unprime;
}

static void etr(struct posdata *pdat)
{
  if (pdat->coszen > 0.0)
  {
    pdat->etrn = pdat->solcon * pdat->erv;
    pdat->etr = pdat->etrn * pdat->coszen;
  }
  else
  {
    pdat->etrn = 0.0;
    pdat->etr = 0.0;
  }
}

static void localtrig(struct posdata *pdat, struct trigdata *tdat)
{
#define SD_MASK (L_ZENETR | L_SSHA | S_SBCF | S_SOLAZM)
#define SL_MASK (L_ZENETR | L_SSHA | S_SBCF | S_SOLAZM)
#define CL_MASK (L_ZENETR | L_SSHA | S_SBCF | S_SOLAZM)
#define CD_MASK (L_ZENETR | L_SSHA | S_SBCF)
#define CH_MASK (L_ZENETR)

  if (tdat->sd < -900.0)
  {
    tdat->sd = 1.0;
    if (pdat->function | CD_MASK)
      tdat->cd = cos(raddeg * pdat->declin);
    if (pdat->function | CH_MASK)
      tdat->ch = cos(raddeg * pdat->hrang);
    if (pdat->function | CL_MASK)
      tdat->cl = cos(raddeg * pdat->latitude);
    if (pdat->function | SD_MASK)
      tdat->sd = sin(raddeg * pdat->declin);
    if (pdat->function | SL_MASK)
      tdat->sl = sin(raddeg * pdat->latitude);
  }
}

static void tilt(struct posdata *pdat)
{
  float ca, cp, ct, sa, sp, st, sz;

  ca = cos(raddeg * pdat->azim);
  cp = cos(raddeg * pdat->aspect);
  ct = cos(raddeg * pdat->tilt);
  sa = sin(raddeg * pdat->azim);
  sp = sin(raddeg * pdat->aspect);
  st = sin(raddeg * pdat->tilt);
  sz = sin(raddeg * pdat->zenref);
  pdat->cosinc = pdat->coszen * ct + sz * st * (ca * cp + sa * sp);

  if (pdat->cosinc > 0.0)
    pdat->etrtilt = pdat->etrn * pdat->cosinc;
  else
    pdat->etrtilt = 0.0;
}

void S_decode(long code, struct posdata *pdat)
{
  if (code & (1L << S_YEAR_ERROR))
    printf("S_decode ==> Please fix the year: %d [1950-2050]\r\n", pdat->year);
  if (code & (1L << S_MONTH_ERROR))
    printf("S_decode ==> Please fix the month: %d\r\n", pdat->month);
  if (code & (1L << S_DAY_ERROR))
    printf("S_decode ==> Please fix the day-of-month: %d\r\n", pdat->day);
  if (code & (1L << S_DOY_ERROR))
    printf("S_decode ==> Please fix the day-of-year: %d\r\n", pdat->daynum);
  if (code & (1L << S_HOUR_ERROR))
    printf("S_decode ==> Please fix the hour: %d\r\n", pdat->hour);
  if (code & (1L << S_MINUTE_ERROR))
    printf("S_decode ==> Please fix the minute: %d\r\n", pdat->minute);
  if (code & (1L << S_SECOND_ERROR))
    printf("S_decode ==> Please fix the second: %d\r\n", pdat->second);
  if (code & (1L << S_TZONE_ERROR))
    printf("S_decode ==> Please fix the time zone: %f\r\n", pdat->timezone);
  if (code & (1L << S_INTRVL_ERROR))
    printf("S_decode ==> Please fix the interval: %d\r\n", pdat->interval);
  if (code & (1L << S_LAT_ERROR))
    printf("S_decode ==> Please fix the latitude: %f\r\n", pdat->latitude);
  if (code & (1L << S_LON_ERROR))
    printf("S_decode ==> Please fix the longitude: %f\r\n", pdat->longitude);
  if (code & (1L << S_TEMP_ERROR))
    printf("S_decode ==> Please fix the temperature: %f\r\n", pdat->temp);
  if (code & (1L << S_PRESS_ERROR))
    printf("S_decode ==> Please fix the pressure: %f\r\n", pdat->press);
  if (code & (1L << S_TILT_ERROR))
    printf("S_decode ==> Please fix the tilt: %f\r\n", pdat->tilt);
  if (code & (1L << S_ASPECT_ERROR))
    printf("S_decode ==> Please fix the aspect: %f\r\n", pdat->aspect);
  if (code & (1L << S_SBWID_ERROR))
    printf("S_decode ==> Please fix the shadowband width: %f\r\n", pdat->sbwid);
  if (code & (1L << S_SBRAD_ERROR))
    printf("S_decode ==> Please fix the shadowband radius: %f\r\n", pdat->sbrad);
  if (code & (1L << S_SBSKY_ERROR))
    printf("S_decode ==> Please fix the shadowband sky factor: %f\r\n", pdat->sbsky);
}
