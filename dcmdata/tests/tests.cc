/*
 *
 *  Copyright (C) 2011, OFFIS e.V.
 *  All rights reserved.  See COPYRIGHT file for details.
 *
 *  This software and supporting documentation were developed by
 *
 *    OFFIS e.V.
 *    R&D Division Health
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *
 *  Module:  dcmdata
 *
 *  Author:  Uli Schlachter
 *
 *  Purpose: main test program
 *
 *  Last Update:      $Author: uli $
 *  Update Date:      $Date: 2011-06-07 07:06:57 $
 *  CVS/RCS Revision: $Revision: 1.2 $
 *  Status:           $State: Exp $
 *
 *  CVS/RCS Log at end of file
 *
 */

#include "dcmtk/config/osconfig.h"

#include "dcmtk/ofstd/oftest.h"

OFTEST_REGISTER(dcmdata_partialElementAccess);
OFTEST_REGISTER(dcmdata_i2d_bmp);
OFTEST_REGISTER(dcmdata_checkStringValue);
OFTEST_REGISTER(dcmdata_pathAcces);
OFTEST_REGISTER(dcmdata_dateTime);
OFTEST_REGISTER(dcmdata_elementLength_EVR_AE);
OFTEST_REGISTER(dcmdata_elementLength_EVR_AS);
OFTEST_REGISTER(dcmdata_elementLength_EVR_AT);
OFTEST_REGISTER(dcmdata_elementLength_EVR_CS);
OFTEST_REGISTER(dcmdata_elementLength_EVR_DA);
OFTEST_REGISTER(dcmdata_elementLength_EVR_DS);
OFTEST_REGISTER(dcmdata_elementLength_EVR_DT);
OFTEST_REGISTER(dcmdata_elementLength_EVR_FD);
OFTEST_REGISTER(dcmdata_elementLength_EVR_FL);
OFTEST_REGISTER(dcmdata_elementLength_EVR_IS);
OFTEST_REGISTER(dcmdata_elementLength_EVR_LO);
OFTEST_REGISTER(dcmdata_elementLength_EVR_LT);
OFTEST_REGISTER(dcmdata_elementLength_EVR_OB);
OFTEST_REGISTER(dcmdata_elementLength_EVR_OF);
OFTEST_REGISTER(dcmdata_elementLength_EVR_OW);
OFTEST_REGISTER(dcmdata_elementLength_EVR_OverlayData);
OFTEST_REGISTER(dcmdata_elementLength_EVR_PN);
OFTEST_REGISTER(dcmdata_elementLength_EVR_PixelData);
OFTEST_REGISTER(dcmdata_elementLength_EVR_SH);
OFTEST_REGISTER(dcmdata_elementLength_EVR_SL);
OFTEST_REGISTER(dcmdata_elementLength_EVR_SQ);
OFTEST_REGISTER(dcmdata_elementLength_EVR_SS);
OFTEST_REGISTER(dcmdata_elementLength_EVR_ST);
OFTEST_REGISTER(dcmdata_elementLength_EVR_TM);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UI);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UL);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UN);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UNKNOWN);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UNKNOWN2B);
OFTEST_REGISTER(dcmdata_elementLength_EVR_US);
OFTEST_REGISTER(dcmdata_elementLength_EVR_UT);
OFTEST_REGISTER(dcmdata_elementLength_EVR_lt);
OFTEST_REGISTER(dcmdata_elementLength_EVR_na);
OFTEST_REGISTER(dcmdata_elementLength_EVR_ox);
OFTEST_REGISTER(dcmdata_elementLength_EVR_up);
OFTEST_REGISTER(dcmdata_elementLength_EVR_xs);
OFTEST_REGISTER(dcmdata_elementLength_pixelItem);
OFTEST_REGISTER(dcmdata_elementLength_pixelSequence);
OFTEST_MAIN("dcmdata")

/*
**
** CVS/RCS Log:
** $Log: tests.cc,v $
** Revision 1.2  2011-06-07 07:06:57  uli
** Added test cases for DcmElement::calcElementLength().
**
** Revision 1.1  2011-05-25 10:05:55  uli
** Imported oftest and converted existing tests to oftest.
**
**
**
*/