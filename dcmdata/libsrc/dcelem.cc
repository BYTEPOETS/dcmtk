/*
 *
 *  Copyright (C) 1994-2008, OFFIS
 *
 *  This software and supporting documentation were developed by
 *
 *    Kuratorium OFFIS e.V.
 *    Healthcare Information and Communication Systems
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *  THIS SOFTWARE IS MADE AVAILABLE,  AS IS,  AND OFFIS MAKES NO  WARRANTY
 *  REGARDING  THE  SOFTWARE,  ITS  PERFORMANCE,  ITS  MERCHANTABILITY  OR
 *  FITNESS FOR ANY PARTICULAR USE, FREEDOM FROM ANY COMPUTER DISEASES  OR
 *  ITS CONFORMITY TO ANY SPECIFICATION. THE ENTIRE RISK AS TO QUALITY AND
 *  PERFORMANCE OF THE SOFTWARE IS WITH THE USER.
 *
 *  Module:  dcmdata
 *
 *  Author:  Gerd Ehlers, Andreas Barth
 *
 *  Purpose: Implementation of class DcmElement
 *
 *  Last Update:      $Author: onken $
 *  Update Date:      $Date: 2008-07-17 10:31:31 $
 *  CVS/RCS Revision: $Revision: 1.62 $
 *  Status:           $State: Exp $
 *
 *  CVS/RCS Log at end of file
 *
 */


#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#define INCLUDE_NEW
#define INCLUDE_CSTDLIB
#define INCLUDE_CSTRING
#include "dcmtk/ofstd/ofstdinc.h"

#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/dcmdata/dcelem.h"
#include "dcmtk/dcmdata/dcobject.h"
#include "dcmtk/dcmdata/dcdefine.h"
#include "dcmtk/dcmdata/dcswap.h"
#include "dcmtk/dcmdata/dcdebug.h"
#include "dcmtk/dcmdata/dcistrma.h"    /* for class DcmInputStream */
#include "dcmtk/dcmdata/dcostrma.h"    /* for class DcmOutputStream */
#include "dcmtk/dcmdata/dcfcache.h"    /* for class DcmFileCache */
#include "dcmtk/dcmdata/dcwcache.h"    /* for class DcmWriteCache */
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmdata/dcdeftag.h"

#define SWAPBUFFER_SIZE 16  /* sufficient for all DICOM VRs as per the 2007 edition */

//
// CLASS DcmElement
//

DcmElement::DcmElement(const DcmTag &tag,
                       const Uint32 len)
  : DcmObject(tag, len),
    fByteOrder(gLocalByteOrder),
    fLoadValue(NULL),
    fValue(NULL)
{
}


DcmElement::DcmElement(const DcmElement &elem)
  : DcmObject(elem),
    fByteOrder(elem.fByteOrder),
    fLoadValue(NULL),
    fValue(NULL)
{
    if (elem.fValue)
    {
        DcmVR vr(elem.getVR());
        const unsigned short pad = (vr.isaString()) ? OFstatic_cast(unsigned short, 1) : OFstatic_cast(unsigned short, 0);

        // The next lines are a special version of newValueField().
        // newValueField() cannot be used because it is virtual and it does
        // not allocate enough bytes for strings. The number of pad bytes
        // is added to the Length for this purpose.
        if (getLengthField() & 1)
        {
            fValue = new Uint8[getLengthField() + 1 + pad]; // protocol error: odd value length
            if (fValue)
                fValue[getLengthField()] = 0;
            setLengthField(getLengthField() + 1);           // make Length even
        }
        else
            fValue = new Uint8[getLengthField() + pad];

        if (!fValue)
            errorFlag = EC_MemoryExhausted;

        if (pad && fValue)
            fValue[getLengthField()] = 0;

        memcpy(fValue, elem.fValue, size_t(getLengthField() + pad));
    }

    if (elem.fLoadValue)
        fLoadValue = elem.fLoadValue->clone();
}


DcmElement &DcmElement::operator=(const DcmElement &obj)
{
  if (this != &obj)
  {
    delete[] fValue;
    delete fLoadValue;
    fLoadValue = NULL;
    fValue = NULL;

    DcmObject::operator=(obj);
    fByteOrder = obj.fByteOrder;

    if (obj.fValue)
    {
        DcmVR vr(obj.getVR());
        const unsigned short pad = (vr.isaString()) ? OFstatic_cast(unsigned short, 1) : OFstatic_cast(unsigned short, 0);

        // The next lines are a special version of newValueField().
        // newValueField() cannot be used because it is virtual and it does
        // not allocate enough bytes for strings. The number of pad bytes
        // is added to the Length for this purpose.

        if (getLengthField() & 1)
        {
            fValue = new Uint8[getLengthField() + 1 + pad]; // protocol error: odd value length
            if (fValue)
                fValue[getLengthField()] = 0;
            setLengthField(getLengthField() + 1);           // make Length even
        }
        else
            fValue = new Uint8[getLengthField() + pad];

        if (!fValue)
            errorFlag = EC_MemoryExhausted;

        if (pad && fValue)
            fValue[getLengthField()] = 0;

        memcpy(fValue, obj.fValue, size_t(getLengthField()+pad));
    }

    if (obj.fLoadValue)
        fLoadValue = obj.fLoadValue->clone();

  }
  return *this;
}


OFCondition DcmElement::copyFrom(const DcmObject& rhs)
{
  if (this != &rhs)
  {
    if (rhs.ident() != ident()) return EC_IllegalCall;
    *this = (DcmElement&) rhs;
  }
  return EC_Normal;
}


DcmElement::~DcmElement()
{
    delete[] fValue;
    delete fLoadValue;
}


// ********************************


OFCondition DcmElement::clear()
{
    errorFlag = EC_Normal;
    delete[] fValue;
    fValue = NULL;
    delete fLoadValue;
    fLoadValue = NULL;
    setLengthField(0);
    return errorFlag;
}


// ********************************


Uint32 DcmElement::calcElementLength(const E_TransferSyntax xfer,
                                     const E_EncodingType enctype)
{
    DcmXfer xferSyn(xfer);
    return getLength(xfer, enctype) + xferSyn.sizeofTagHeader(getVR());
}


OFBool DcmElement::canWriteXfer(const E_TransferSyntax newXfer,
                                const E_TransferSyntax /*oldXfer*/)
{
    return newXfer != EXS_Unknown;
}


OFCondition DcmElement::detachValueField(OFBool copy)
{
    OFCondition l_error = EC_Normal;
    if (getLengthField() != 0)
    {
        if (copy)
        {
            if (!fValue)
                l_error = loadValue();
            Uint8 * newValue = new Uint8[getLengthField()];
            memcpy(newValue, fValue, size_t(getLengthField()));
            fValue = newValue;
        } else {
            fValue = NULL;
            setLengthField(0);
        }
    }
    return l_error;
}


// ********************************


OFCondition DcmElement::getUint8(Uint8 & /*val*/,
                                 const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getSint16(Sint16 & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getUint16(Uint16 & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getSint32(Sint32 & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getUint32(Uint32 & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getFloat32(Float32 & /*val*/,
                                   const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getFloat64(Float64 & /*val*/,
                                   const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getTagVal(DcmTagKey & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getOFString(OFString &/*val*/,
                                    const unsigned long /*pos*/,
                                    OFBool /*normalize*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getString(char * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getOFStringArray(OFString &value,
                                         OFBool normalize)
{
    /* this is a general implementation which is only used when the derived
       VR class does not reimplement it
     */
    errorFlag = EC_Normal;
    const unsigned long count = getVM();
    /* intialize result string */
    value.clear();
    if (count > 0)
    {
        OFString string;
        unsigned long i = 0;
        /* reserve number of bytes expected (heuristic) */
        value.reserve(OFstatic_cast(unsigned int, getLength()));
        /* iterate over all values and convert them to a character string */
        while ((i < count) && (errorFlag = getOFString(string, i, normalize)).good())
        {
            if (i > 0)
                value += '\\';
            /* append current value to the result string */
            value += string;
            i++;
        }
    }
    return errorFlag;
}


OFCondition DcmElement::getUint8Array(Uint8 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getSint16Array(Sint16 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getUint16Array(Uint16 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getSint32Array(Sint32 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getUint32Array(Uint32 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getFloat32Array(Float32 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::getFloat64Array(Float64 * &/*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


void *DcmElement::getValue(const E_ByteOrder newByteOrder)
{
    /* initialize return value */
    Uint8 * value = NULL;
    /* if the byte ordering is unknown, this is an illegal call */
    if (newByteOrder == EBO_unknown)
        errorFlag = EC_IllegalCall;
    else
    {
        /* in case this call is not illegal, we need to do something. First of all, set the error flag to ok */
        errorFlag =  EC_Normal;
        /* do something only if the length of this element's value does not equal (i.e. is greater than) 0 */
        if (getLengthField() != 0)
        {
            /* if the value has not yet been loaded, do so now */
            if (!fValue)
                errorFlag = loadValue();
            /* �f everything is ok */
            if (errorFlag.good())
            {
                /* if this element's value's byte ordering does not correspond to the */
                /* desired byte ordering, we need to rearrange this value's bytes and */
                /* set its byte order indicator variable correspondingly */
                if (newByteOrder != fByteOrder)
                {
                    swapIfNecessary(newByteOrder, fByteOrder, fValue,
                                    getLengthField(), getTag().getVR().getValueWidth());
                    fByteOrder = newByteOrder;
                }
                /* if everything is ok, assign the current value to the result variable */
                if (errorFlag.good())
                    value = fValue;
            }
        }
    }
    /* return result */
    return value;
}


// ********************************


OFCondition DcmElement::loadAllDataIntoMemory()
{
    errorFlag = EC_Normal;
    if (!fValue && (getLengthField() != 0))
        errorFlag = loadValue();
    return errorFlag;
}


OFCondition DcmElement::loadValue(DcmInputStream *inStream)
{
    /* initiailze return value */
    errorFlag = EC_Normal;
    /* only if the length of this element does not equal 0, read information */
    if (getLengthField() != 0)
    {
        DcmInputStream *readStream = inStream;
        OFBool isStreamNew = OFFalse;

        // if the NULL pointer was passed (i.e. we're not in the middle of
        // a read() cycle, and fValue is NULL (i.e. the attribute value still
        // remains in file and fLoadValue is not NULL (i.e. we know how to
        // load the value from that file, then let's do it..
        if (!readStream && fLoadValue && !fValue)
        {
            /* we need to read information from the stream which is */
            /* accessible through fLoadValue. Hence, reassign readStream */
            readStream = fLoadValue->create();

            /* set isStreamNew to OFTrue */
            isStreamNew = OFTrue;

            /* reset number of transferred bytes to zero */
            setTransferredBytes(0);            
        }
        /* if we have a stream from which we can read */
        if (readStream)
        {
            /* check if the stream reported an error */
            errorFlag = readStream->status();
            /* if we encountered the end of the stream, set the error flag correspondingly */
            if (errorFlag.good() && readStream->eos())
                errorFlag = EC_EndOfStream;
            /* if we did not encounter the end of the stream and no error occured so far, go ahead */
            else if (errorFlag.good())
            {
                /* if the object which holds this element's value does not yet exist, create it */
                if (!fValue)
                    fValue = newValueField();

                /* if there is still no such object, the memory must be exhausted */
                if (!fValue)
                    errorFlag = EC_MemoryExhausted;
                /* else (i.e. we have an object which can be used to capture this element's */
                /* value) we need to read a certain amount of bytes from the stream */
                else
                {
                    /* determine how many bytes shall be read from the stream */
                    Uint32 readLength = getLengthField() - getTransferredBytes();

                    /* read a corresponding amount of bytes from the stream, store the information in fvalue */
                    /* increase the counter that counts how many bytes were actually read */
                    incTransferredBytes(readStream->read(&fValue[getTransferredBytes()], readLength));

                    /* if we have read all the bytes which make up this element's value */
                    if (getLengthField() == getTransferredBytes())
                    {
                        /* call a function which performs certain operations on the information which was read */
                        postLoadValue();
                        errorFlag = EC_Normal;
                    }
                    /* else set the return value correspondingly */
                    else if (readStream->eos())
                        errorFlag = EC_InvalidStream; // premature end of stream
                    else
                        errorFlag = EC_StreamNotifyClient;
                }
            }
            /* if we created the stream from which information was read in this */
            /* function, we need to we need to delete this object here as well */
            if (isStreamNew)
                delete readStream;
        }
    }
    /* return result value */
    return errorFlag;
}


// ********************************


Uint8 *DcmElement::newValueField()
{
    Uint8 * value;
    /* if this element's lenght is odd */
    if (getLengthField() & 1)
    {
        /* create an array of Length+1 bytes */
#ifdef HAVE_STD__NOTHROW
        // we want to use a non-throwing new here if available.
        // If the allocation fails, we report an EC_MemoryExhausted error
        // back to the caller.
        value = new (std::nothrow) Uint8[getLengthField() + 1];    // protocol error: odd value length
#else
        value = new Uint8[getLengthField() + 1];    // protocol error: odd value length
#endif
        /* if creation was successful, set last byte to 0 (in order to initialize this byte) */
        /* (no value will be assigned to this byte later, since Length was odd) */
        if (value)
            value[getLengthField()] = 0;
        /* enforce old (pre DCMTK 3.5.2) behaviour ? */
        if (! dcmAcceptOddAttributeLength.get())
        {
            setLengthField(getLengthField() + 1);           // make Length even
        }
    }
    /* if this element's length is even, create a corresponding array of Lenght bytes */
    else
#ifdef HAVE_STD__NOTHROW
        // we want to use a non-throwing new here if available.
        // If the allocation fails, we report an EC_MemoryExhausted error
        // back to the caller.
        value = new (std::nothrow) Uint8[getLengthField()];
#else
        value = new Uint8[getLengthField()];
#endif
    /* if creation was not successful set member error flag correspondingly */
    if (!value)
        errorFlag = EC_MemoryExhausted;
    /* return byte array */
    return value;
}


// ********************************


void DcmElement::postLoadValue()
{
    if (dcmEnableAutomaticInputDataCorrection.get())
    {
        // newValueField always allocates an even number of bytes
        // and sets the pad byte to zero, so we can safely increase Length here
        if (getLengthField() & 1)
            setLengthField(getLengthField() + 1);           // make Length even
    }
}


// ********************************


OFCondition DcmElement::changeValue(const void *value,
                                    const Uint32 position,
                                    const Uint32 num)
{
    OFBool done = OFFalse;
    errorFlag = EC_Normal;
    if (position % num != 0 || getLengthField() % num != 0 || position > getLengthField())
        errorFlag = EC_IllegalCall;
    else if (position == getLengthField())
    {
        if (getLengthField() == 0)
        {
            errorFlag = putValue(value, num);
            done = OFTrue;
        } else {
            // load value (if not loaded yet)
            if (!fValue)
                loadValue();
            // allocate new memory for value
            Uint8 * newValue = new Uint8[getLengthField() + num];
            if (!newValue)
                errorFlag = EC_MemoryExhausted;
            if (errorFlag.good())
            {
                // swap to local byte order
                swapIfNecessary(gLocalByteOrder, fByteOrder, fValue,
                                getLengthField(), getTag().getVR().getValueWidth());
                fByteOrder = gLocalByteOrder;
                // copy old value in the beginning of new value
                memcpy(newValue, fValue, size_t(getLengthField()));
                // set parameter value in the extension
                memcpy(&newValue[getLengthField()], OFstatic_cast(const Uint8 *, value), size_t(num));
                delete[] fValue;
                fValue = newValue;
                setLengthField(getLengthField() + num);
            }
            done = OFTrue;
        }
    }
    // copy value at position
    if (!done && errorFlag.good())
    {
        // swap to local byte order
        swapIfNecessary(gLocalByteOrder, fByteOrder, fValue,
                        getLengthField(), getTag().getVR().getValueWidth());
        memcpy(&fValue[position], OFstatic_cast(const Uint8 *, value), size_t(num));
        fByteOrder = gLocalByteOrder;
    }
    return errorFlag;
}


// ********************************


OFCondition DcmElement::putOFStringArray(const OFString& /* stringValue*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putString(const char * /*val*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putSint16(const Sint16 /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putUint16(const Uint16 /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putSint32(const Sint32 /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putUint32(const Uint32 /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putFloat32(const Float32 /*val*/,
                                   const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putFloat64(const Float64 /*val*/,
                                   const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putTagVal(const DcmTagKey & /*val*/,
                                  const unsigned long /*pos*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putUint8Array(const Uint8 * /*val*/,
                                      const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putSint16Array(const Sint16 * /*val*/,
                                       const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putUint16Array(const Uint16 * /*val*/,
                                       const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putSint32Array(const Sint32 * /*val*/,
                                       const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putUint32Array(const Uint32 * /*val*/,
                                       const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putFloat32Array(const Float32 * /*val*/,
                                        const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putFloat64Array(const Float64 * /*val*/,
                                        const unsigned long /*num*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::putValue(const void * newValue,
                                 const Uint32 length)
{
    errorFlag = EC_Normal;

    if (fValue)
        delete[] fValue;
    fValue = NULL;

    if (fLoadValue)
        delete fLoadValue;
    fLoadValue = NULL;

    setLengthField(length);

    if (length != 0)
    {
        fValue = newValueField();

        // newValueField always allocates an even number of bytes
        // and sets the pad byte to zero, so we can safely increase Length here
        if (getLengthField() & 1)
            setLengthField(getLengthField() + 1);           // make Length even

        // copy length (which may be odd), not Length (which is always even)
        if (fValue)
            memcpy(fValue, newValue, size_t(length));
        else
            errorFlag = EC_MemoryExhausted;
    }
    fByteOrder = gLocalByteOrder;
    return errorFlag;
}


// ********************************


OFCondition DcmElement::createUint8Array(const Uint32 /*numBytes*/,
                                         Uint8 *& /*bytes*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


OFCondition DcmElement::createUint16Array(const Uint32 /*numWords*/,
                                          Uint16 *& /*bytes*/)
{
    errorFlag = EC_IllegalCall;
    return errorFlag;
}


// ********************************


OFCondition DcmElement::createEmptyValue(const Uint32 length)
{
    errorFlag = EC_Normal;
    if (fValue)
        delete[] fValue;
    fValue = NULL;
    if (fLoadValue)
        delete fLoadValue;
    fLoadValue = NULL;
    setLengthField(length);

    if (length != 0)
    {
        fValue = newValueField();
        // newValueField always allocates an even number of bytes
        // and sets the pad byte to zero, so we can safely increase Length here
        if (getLengthField() & 1)
            setLengthField(getLengthField() + 1);           // make Length even

        // initialize <length> bytes (which may be odd), not Length (which is always even)
        if (fValue)
            memzero(fValue, size_t(length));
        else
            errorFlag = EC_MemoryExhausted;
    }
    fByteOrder = gLocalByteOrder;
    return errorFlag;
}


// ********************************


OFCondition DcmElement::read(DcmInputStream &inStream,
                             const E_TransferSyntax ixfer,
                             const E_GrpLenEncoding /*glenc*/,
                             const Uint32 maxReadLength)
{
    /* if this element's transfer state shows ERW_notInitialized, this is an illegal call */
    if (getTransferState() == ERW_notInitialized)
        errorFlag = EC_IllegalCall;
    else
    {
        /* if this is not an illegal call, go ahead and create a DcmXfer */
        /* object based on the transfer syntax which was passed */
        DcmXfer inXfer(ixfer);
        /* determine the transfer syntax's byte ordering */
        fByteOrder = inXfer.getByteOrder();
        /* check if the stream reported an error */
        errorFlag = inStream.status();
        /* if we encountered the end of the stream, set the error flag correspondingly */
        if (errorFlag.good() && inStream.eos())
            errorFlag = EC_EndOfStream;
        /* if we did not encounter the end of the stream and no error occured so far, go ahead */
        else if (errorFlag.good())
        {
            /* if the transfer state is ERW_init, we need to prepare */
            /* the reading of this element's value from the stream */
            if (getTransferState() == ERW_init)
            {
                /* if the Length of this element's value is greater than the amount of bytes we */
                /* can read from the stream and if the stream has random access, we want to create */
                /* a DcmInputStreamFactory object that enables us to read this element's value later. */
                /* This new object will be stored (together with the position where we have to start */
                /* reading the value) in the member variable fLoadValue. */
                if (getLengthField() > maxReadLength)
                {
                    /* try to create a stream factory to read the value later */
                    delete fLoadValue;
                    fLoadValue = inStream.newFactory();

                    if (fLoadValue)
                    {
                        Uint32 skipped = inStream.skip(getLengthField());
                        if (skipped < getLengthField())
                        {
                            errorFlag = EC_InvalidStream;  // attribute larger than remaining bytes in file
                            /* Print an error message when too few bytes are available in the file in order to
                             * distinguish this problem from any other generic "InvalidStream" problem. */
                            ofConsole.lockCerr() << "DcmElement: " << getTagName() << getTag().getXTag() << " larger ("
                                                 << getLengthField() << ") than remaining bytes in file" << OFendl;
                            ofConsole.unlockCerr();
                        }
                    }
                }
                /* if there is already a value for this element, delete this value */
                delete[] fValue;
                /* set the transfer state to ERW_inWork */
                setTransferState(ERW_inWork);
            }
            /* if the transfer state is ERW_inWork and we are not supposed */
            /* to read this element's value later, read the value now */
            if (getTransferState() == ERW_inWork && !fLoadValue)
                errorFlag = loadValue(&inStream);
            /* if the amount of transferred bytes equals the Length of this element */
            /* or the object which contains information to read the value of this */
            /* element later is existent, set the transfer state to ERW_ready */
            if (getTransferredBytes() == getLengthField() || fLoadValue)
                setTransferState(ERW_ready);
        }
    }

    /* return result value */
    return errorFlag;
}


// ********************************


void DcmElement::swapValueField(size_t valueWidth)
{
    if (getLengthField() != 0)
    {
        if (!fValue)
            errorFlag = loadValue();

        if (errorFlag.good())
            swapBytes(fValue, getLengthField(), valueWidth);
    }
}


// ********************************


void DcmElement::transferInit()
{
    DcmObject::transferInit();
    setTransferredBytes(0);
}


// ********************************

OFCondition DcmElement::write(
    DcmOutputStream &outStream,
    const E_TransferSyntax oxfer,
    const E_EncodingType /*enctype*/,
    DcmWriteCache *wcache)
{
    DcmWriteCache wcache2;

    /* In case the transfer state is not initialized, this is an illegal call */
    if (getTransferState() == ERW_notInitialized)
        errorFlag = EC_IllegalCall;
    else
    {
        /* if this is not an illegal call, we need to do something. First */
        /* of all, check the error state of the stream that was passed */
        /* only do something if the error state of the stream is ok */
        errorFlag = outStream.status();
        if (errorFlag.good())
        {
            /* create an object that represents the transfer syntax */
            DcmXfer outXfer(oxfer);

            /* pointer to element value if value resides in memory or old-style
             * write behaviour is active (i.e. everything loaded into memory upon write 
             */
            Uint8 *value = NULL;
            OFBool accessPossible = OFFalse;

            /* check that we actually do have access to the element's value.
             * If the element is unaccessable (which would mean that the value resides
             * in file and access to the file fails), write an empty element with
             * zero length.
             */
            if (getLengthField() > 0)
            {
              if (valueLoaded())
              {
                /* get this element's value. Mind the byte ordering (little */
                /* or big endian) of the transfer syntax which shall be used */
                value = OFstatic_cast(Uint8 *, getValue(outXfer.getByteOrder()));
                if (value) accessPossible = OFTrue;
              }
              else
              {
                /* Use local cache object if needed. This may cause those bytes
                 * that are read but not written because the output stream stalls to
                 * be read again, and the file from being re-opened next time.
                 * Therefore, this case should be avoided.
                 */
                if (! wcache) wcache = &wcache2; 

                /* initialize cache object. This is safe even if the object was already initialized */
                wcache->init(this, getLengthField(), getTransferredBytes(), outXfer.getByteOrder());

                /* access first block of element content */
                errorFlag = wcache->fillBuffer(*this);

                /* check that everything worked and the buffer is non-empty now */
                accessPossible = errorFlag.good() && (! wcache->bufferIsEmpty());
              }
            }

            /* if this element's transfer state is ERW_init (i.e. it has not yet been written to */
            /* the stream) and if the outstream provides enough space for tag and length information */
            /* write tag and length information to it, do something */
            if (getTransferState() == ERW_init)
            {
                /* first compare with DCM_TagInfoLength (12). If there is not enough space
                 * in the buffer, check if the buffer is still sufficient for the requirements
                 * of this element, which may need only 8 instead of 12 bytes.
                 */
                if ((outStream.avail() >= DCM_TagInfoLength) ||
                    (outStream.avail() >= getTagAndLengthSize(oxfer)))
                {
                    /* if there is no value, Length (member variable) shall be set to 0 */
                    if (! accessPossible) setLengthField(0);

                    /* remember how many bytes have been written to the stream, currently none so far */
                    Uint32 writtenBytes = 0;

                    /* write tag and length information (and possibly also data type information) to the stream, */
                    /* mind the transfer syntax and remember the amount of bytes that have been written */
                    errorFlag = writeTagAndLength(outStream, oxfer, writtenBytes);

                    /* if the writing was successful, set this element's transfer */
                    /* state to ERW_inWork and the amount of transferred bytes to 0 */
                    if (errorFlag.good())
                    {
                        setTransferState(ERW_inWork);
                        setTransferredBytes(0);
                    }
                } else errorFlag = EC_StreamNotifyClient;
            }

            /* if there is a value that has to be written to the stream */
            /* and if this element's transfer state is ERW_inWork */
            if (accessPossible && getTransferState() == ERW_inWork)
            {
                Uint32 len = 0;
                if (valueLoaded())
                {
                    /* write as many bytes as possible to the stream starting at value[getTransferredBytes()] */
                    /* (note that the bytes value[0] to value[getTransferredBytes()-1] have already been */
                    /* written to the stream) */
                    len = outStream.write(&value[getTransferredBytes()], getLengthField() - getTransferredBytes());

                    /* increase the amount of bytes which have been transfered correspondingly */
                    incTransferredBytes(len);
                    
                    /* see if there is something fishy with the stream */
                    errorFlag = outStream.status();
                }
                else
                {
                    Uint32 buflen = 0;
                    OFBool done = getTransferredBytes() == getLengthField();
                    while (! done)
                    {
                      // re-fill buffer from file if empty
                      errorFlag = wcache->fillBuffer(*this);
                      buflen = wcache->contentLength();
                    
                      if (errorFlag.good())
                      {
                        // write as many bytes from cache buffer to stream as possible
                        len = wcache->writeBuffer(outStream);

                        /* increase the amount of bytes which have been transfered correspondingly */
                        incTransferredBytes(len);

                        /* see if there is something fishy with the stream */
                        errorFlag = outStream.status();
                      }
                    
                      // stop writing if something went wrong, we were unable to send all of the buffer content
                      // (which indicates that the output stream needs to be flushed, or everything was sent out.
                      done = errorFlag.bad() || (len < buflen) || (getTransferredBytes() == getLengthField());
                    }
                }

                /* if the amount of transferred bytes equals the length of the element's value, the */
                /* entire value has been written to the stream. Thus, this element's transfer state */
                /* has to be set to ERW_ready. If this is not the case but the error flag still shows */
                /* an ok value, there was no more space in the stream and a corresponding return value */
                /* has to be set. (Isn't the "else if" part superfluous?!?) */
                if (getLengthField() == getTransferredBytes()) setTransferState(ERW_ready);
                else if (errorFlag.good()) errorFlag = EC_StreamNotifyClient;

            }
        }
    }
    /* return result value */
    return errorFlag;
}


OFCondition DcmElement::writeSignatureFormat(
   DcmOutputStream &outStream,
   const E_TransferSyntax oxfer,
   const E_EncodingType enctype,
   DcmWriteCache *wcache)                           
{
    // for normal DICOM elements (everything except sequences), the data
    // stream used for digital signature creation or verification is
    // identical to the stream used for network communication or media
    // storage.
    return write(outStream, oxfer, enctype, wcache);
}


// ********************************


void DcmElement::writeXMLStartTag(STD_NAMESPACE ostream &out,
                                  const size_t flags,
                                  const char *attrText)
{
    OFString xmlString;
    DcmVR vr(getTag().getVR());
    /* write standardized XML start tag for all element types */
    out << "<element";
    /* attribute tag = (gggg,eeee) */
    out << " tag=\"";
    out << STD_NAMESPACE hex << STD_NAMESPACE setfill('0')
        << STD_NAMESPACE setw(4) << getTag().getGTag() << ","
        << STD_NAMESPACE setw(4) << getTag().getETag() << "\""
        << STD_NAMESPACE dec << STD_NAMESPACE setfill(' ');
    /* value representation = VR */
    out << " vr=\"" << vr.getVRName() << "\"";
    /* value multiplicity = 1..n */
    out << " vm=\"" << getVM() << "\"";
    /* value length in bytes = 0..max */
    out << " len=\"" << getLengthField() << "\"";
    /* tag name (if known and not suppressed) */
    if (!(flags & DCMTypes::XF_omitDataElementName))
        out << " name=\"" << OFStandard::convertToMarkupString(getTagName(), xmlString) << "\"";
    /* value loaded = no (or absent)*/
    if (!valueLoaded())
        out << " loaded=\"no\"";
    /* write additional attributes (if any) */
    if ((attrText != NULL) && (strlen(attrText) > 0))
        out << " " << attrText;
    out << ">";
}


void DcmElement::writeXMLEndTag(STD_NAMESPACE ostream &out,
                                const size_t /*flags*/)
{
    /* write standardized XML end tag for all element types */
    out << "</element>" << OFendl;
}


OFCondition DcmElement::writeXML(STD_NAMESPACE ostream &out,
                                 const size_t flags)
{
    /* XML start tag: <element tag="gggg,eeee" vr="XX" ...> */
    writeXMLStartTag(out, flags);
    /* write element value (if loaded) */
    if (valueLoaded())
    {
        OFString value, xmlString;
        if (getOFStringArray(value).good())
        {
            /* check whether conversion to XML markup string is required */
            if (OFStandard::checkForMarkupConversion(value))
                out << OFStandard::convertToMarkupString(value, xmlString);
            else
                out << value;
        }
    }
    /* XML end tag: </element> */
    writeXMLEndTag(out, flags);
    /* always report success */
    return EC_Normal;
}


OFCondition DcmElement::getPartialValue(
  void *targetBuffer, 
  offile_off_t offset, 
  offile_off_t numBytes, 
  DcmFileCache *cache,
  E_ByteOrder byteOrder)
{  
  // check integrity of parameters passed to this method
  if (targetBuffer == NULL) return EC_IllegalCall;

  // if the user has only requested zero bytes, we immediately return
  if (numBytes == 0) return EC_Normal;
  	
  // offset must always be less then attribute length (unless offset,
  // attribute length and numBytes are all zero, a case that was
  // handled above).
  if (offset >= getLengthField()) return EC_InvalidOffset;

  // check if the caller is trying to read past the end of the value field
  if (numBytes > (getLengthField() - offset)) return EC_TooManyBytesRequested;

  // check if the value is already in memory 
  if (valueLoaded())
  {
    // the attribute value is already in memory. 
    // change internal byte order of the attribute value to the desired byte order.
    // This should only happen once for multiple calls to this method since the
    // caller will hopefully always request the same byte order.
    char *value = OFstatic_cast(char *, getValue(byteOrder));
    if (value)
    {
      memcpy(targetBuffer, value+offset, OFstatic_cast(size_t, numBytes));
    }
    else
    {
      // this should never happen because valueLoaded() returned true, but 
      // we don't want to dereference a NULL pointer anyway
      return EC_IllegalCall;
    }
  }
  else
  {
    // the value is not in memory. We should directly read from file and
    // also consider byte order.

    // since the value is not in memory, fLoadValue should exist. Check anyway.
    if (! fLoadValue) return EC_IllegalCall;

    // make sure we have a file cache object
    DcmFileCache defaultcache; // automatic object, creation is cheap.
    if (cache == NULL) cache = &defaultcache;

    // the stream from which we will read the attribute value
    DcmInputStream *readStream = NULL;

    // check if we need to seek to a position in file earlier than
    // the one specified by the user in order to correctly swap according
    // to the VR.
    size_t valueWidth = getTag().getVR().getValueWidth();

    // we need to cast the target buffer to something we can increment bytewise
    char *targetBufferChar = OFreinterpret_cast(char *, targetBuffer);

    // the swap buffer should be large enough to keep one value of the current VR
    unsigned char swapBuffer[SWAPBUFFER_SIZE];
    if (valueWidth > SWAPBUFFER_SIZE) return EC_IllegalCall;
    	
    // seekoffset is the number of bytes we need to skip from the beginning of the
    // value field to the point where we will start reading. This is always at the
    // start of a new value of a multi-valued attribute.
    size_t partialoffset = offset % valueWidth; 
    size_t partialvalue = 0; 
    offile_off_t seekoffset = offset - partialoffset;
    
    // check if cache already contains the stream we're looking for
    if (cache->isUser(this)) 
    {
      readStream = cache->getStream();

      // since we cannot seek back in the stream (only forward), check if the stream
      // is already past our needed start position
      if (readStream->tell() - cache->getOffset() > seekoffset)
      {
        readStream = NULL;
      } 
    }

    // initialize the cache with new stream
    if (!readStream) 
    {
      readStream = fLoadValue->create();

      // check that read stream is non-NULL
      if (readStream == NULL) return EC_InvalidStream;

      // check that stream status is OK      	
      if (readStream->status().bad()) return readStream->status();

      cache->init(readStream, this);
    }

    // now skip bytes from our current position in file to where we 
    // want to start reading.
    offile_off_t remainingBytesToSkip = seekoffset - (readStream->tell() - cache->getOffset());
    offile_off_t skipResult;

    while (remainingBytesToSkip) 
    {
      skipResult = readStream->skip(remainingBytesToSkip);
      if (skipResult == 0) return EC_InvalidStream; // error while skipping
      remainingBytesToSkip -= skipResult;
    }

    // check if the first few bytes we want to read are "in the middle" of one value
    // of a multi-valued attribute. In that case we need to read the complete value,
    // swap it and then copy only the last bytes in desired byte order.
    if (partialoffset > 0)
    {
      // we possibly want to reset the stream to this point later
      readStream->mark();

      // compute the number of bytes we need to copy from the first attributes 
      partialvalue = valueWidth - partialoffset;
      
      // we need to read a single data element into the swap buffer
      if (valueWidth != OFstatic_cast(size_t, readStream->read(swapBuffer, valueWidth)))
      	return EC_InvalidStream;

      // swap to desired byte order. fByteOrder contains the byte order in file.
      swapIfNecessary(byteOrder, fByteOrder, swapBuffer, valueWidth, valueWidth);

      // copy to target buffer and adjust values
      if (partialvalue > OFstatic_cast(size_t, numBytes))
      {
        memcpy(targetBufferChar, &swapBuffer[partialoffset], numBytes);
        targetBufferChar += numBytes;
        numBytes = 0;

        // Reset stream to position marked before, since we have not copied the complete value
        readStream->putback();
      }
      else
      {
        memcpy(targetBufferChar, &swapBuffer[partialoffset], partialvalue);
        targetBufferChar += partialvalue;
        numBytes -= partialvalue;
      }
    }

    // now read the main block of data directly into the target buffer
    partialvalue = numBytes % valueWidth;
    offile_off_t bytesToRead = numBytes - partialvalue;

    if (bytesToRead > 0)
    {
      // here we read the main block of data
     if (bytesToRead != readStream->read(targetBufferChar, bytesToRead))
         return EC_InvalidStream;

      // swap to desired byte order. fByteOrder contains the byte order in file.
      swapIfNecessary(byteOrder, fByteOrder, targetBufferChar, bytesToRead, valueWidth);

      // adjust pointer to target buffer
      targetBufferChar += bytesToRead;
    }

    // check if the last few bytes we want to read are only a partial value.
    // In that case we need to read the complete value, swap it and then copy
    // only the first few bytes in desired byte order.
    if (partialvalue > 0)
    {
      // we want to reset the stream to this point later
      readStream->mark();

      // we need to read a single data element into the swap buffer
      if (valueWidth != OFstatic_cast(size_t, readStream->read(swapBuffer, valueWidth)))
      	return EC_InvalidStream;

      // swap to desired byte order. fByteOrder contains the byte order in file.
      swapIfNecessary(byteOrder, fByteOrder, swapBuffer, valueWidth, valueWidth);

      // copy to target buffer and adjust values
      memcpy(targetBufferChar, swapBuffer, partialvalue);

      // finally reset stream to position marked before
      readStream->putback();
    }
  }

  // done.
  return EC_Normal;
}  


void DcmElement::compact()
{
  if (fLoadValue && fValue)
  {
    delete[] fValue;
    fValue = NULL;
    setTransferredBytes(0);       
  }
}

OFCondition DcmElement::createValueFromTempFile(
  DcmInputStreamFactory *factory, 
  const Uint32 length,
  const E_ByteOrder byteOrder)
{
    if (factory && (0 == length & 1))
    {
        delete[] fValue;
        fValue = 0;
        delete fLoadValue;
        fLoadValue = factory;
        fByteOrder = byteOrder;
        setLengthField(length);
        return EC_Normal;
    } 
    else return EC_IllegalCall;
}

OFCondition DcmElement::getUncompressedFrameSize(
       DcmItem *dataset,
       Uint32 & frameSize) const
{
  if (dataset == NULL) return EC_IllegalCall;
  Uint16 rows = 0; 
  Uint16 cols = 0;
  Uint16 samplesPerPixel = 0;
  Uint16 bitsAllocated = 0;
  // retrieve values from dataset
  OFCondition result = EC_Normal;
  if (result.good()) result = dataset->findAndGetUint16(DCM_Columns, cols);
  if (result.good()) result = dataset->findAndGetUint16(DCM_Rows, rows);
  if (result.good()) result = dataset->findAndGetUint16(DCM_SamplesPerPixel, samplesPerPixel);
  if (result.good()) result = dataset->findAndGetUint16(DCM_BitsAllocated, bitsAllocated);
  // compute frame size
  Uint16 bytesAllocated = (bitsAllocated > 8) ? 2 : 1;
  frameSize = bytesAllocated * rows * cols * samplesPerPixel;

  // make sure we have enough room for correctly byte-swapping the pixel data if needed
  if (frameSize & 1) ++frameSize;
  return result;
}

OFCondition DcmElement::getUncompressedFrame(
        DcmItem * /* dataset */ ,
        Uint32 /* frameNo */ ,
        Uint32& /* startFragment */ ,
        void * /* buffer */ ,
        Uint32 /* bufSize */ ,
        OFString& /* decompressedColorModel */ ,
        DcmFileCache * /* cache */ )
{
  return EC_IllegalCall;
}

/*
** CVS/RCS Log:
** $Log: dcelem.cc,v $
** Revision 1.62  2008-07-17 10:31:31  onken
** Implemented copyFrom() method for complete DcmObject class hierarchy, which
** permits setting an instance's value from an existing object. Implemented
** assignment operator where necessary.
**
** Revision 1.61  2008-05-29 10:43:20  meichel
** Implemented new method createValueFromTempFile that allows the content of
**   a temporary file to be set as the new value of a DICOM element.
**   Also added a new method compact() that removes the value field if the
**   value field can still be reconstructed from file. For large attribute
**   value the file reference is now kept in memory even when the value has
**   been loaded once. Finally, added new helper method getUncompressedFrameSize
**   that computes the size of an uncompressed frame for a given dataset.
**
** Revision 1.60  2007/11/29 14:30:21  meichel
** Write methods now handle large raw data elements (such as pixel data)
**   without loading everything into memory. This allows very large images to
**   be sent over a network connection, or to be copied without ever being
**   fully in memory.
**
** Revision 1.59  2007/11/23 15:42:36  meichel
** Copy assignment operators in dcmdata now safe for self assignment
**
** Revision 1.58  2007/07/11 08:50:21  meichel
** Initial release of new method DcmElement::getPartialValue which gives access
**   to partial attribute values without loading the complete attribute value
**   into memory, if kept in file.
**
** Revision 1.57  2007/06/29 14:17:49  meichel
** Code clean-up: Most member variables in module dcmdata are now private,
**   not protected anymore.
**
** Revision 1.56  2007/06/07 09:03:17  joergr
** Added createUint8Array() and createUint16Array() methods.
**
** Revision 1.55  2007/02/20 13:19:25  joergr
** Fixed wrong spelling in error message.
**
** Revision 1.54  2006/10/13 10:07:49  joergr
** Added new helper function that allows to check whether the conversion to an
** HTML/XML markup string is required.
**
** Revision 1.53  2006/08/15 15:49:54  meichel
** Updated all code in module dcmdata to correctly compile when
**   all standard C++ classes remain in namespace std.
**
** Revision 1.52  2006/05/11 08:48:35  joergr
** Added new option that allows to omit the element name in the XML output.
**
** Revision 1.51  2005/12/08 15:41:08  meichel
** Changed include path schema for all DCMTK header files
**
** Revision 1.50  2005/07/27 09:31:45  joergr
** Fixed bug in getOFStringArray() which prevented the result string from being
** cleared under certain circumstances.
**
** Revision 1.49  2004/04/27 09:21:27  wilkens
** Fixed a bug in dcelem.cc which occurs when one is serializing a dataset
** (that contains an attribute whose length value is coded with 2 bytes) into
** a given buffer. Although the number of available bytes in the buffer was
** sufficient, the dataset->write(...) method would always return
** EC_StreamNotifyClient to indicate that there are not sufficient bytes
** available in the buffer. This code modification fixes the problem.
**
** Revision 1.48  2004/02/04 16:29:00  joergr
** Adapted type casts to new-style typecast operators defined in ofcast.h.
**
** Revision 1.47  2003/12/11 13:40:46  meichel
** newValueField() now uses std::nothrow new if available
**
** Revision 1.46  2003/10/15 16:55:43  meichel
** Updated error messages for parse errors
**
** Revision 1.45  2003/03/21 13:08:04  meichel
** Minor code purifications for warnings reported by MSVC in Level 4
**
** Revision 1.44  2002/12/09 09:30:50  wilkens
** Modified/Added doc++ documentation.
**
** Revision 1.43  2002/12/06 13:15:12  joergr
** Enhanced "print()" function by re-working the implementation and replacing
** the boolean "showFullData" parameter by a more general integer flag.
** Made source code formatting more consistent with other modules/files.
**
** Revision 1.42  2002/11/27 12:06:46  meichel
** Adapted module dcmdata to use of new header file ofstdinc.h
**
** Revision 1.41  2002/09/17 13:00:11  meichel
** Improved one error code return
**
** Revision 1.40  2002/08/27 16:55:46  meichel
** Initial release of new DICOM I/O stream classes that add support for stream
**   compression (deflated little endian explicit VR transfer syntax)
**
** Revision 1.39  2002/07/08 14:44:39  meichel
** Improved dcmdata behaviour when reading odd tag length. Depending on the
**   global boolean flag dcmAcceptOddAttributeLength, the parser now either accepts
**   odd length attributes or implements the old behaviour, i.e. assumes a real
**   length larger by one.
**
** Revision 1.38  2002/04/25 10:15:09  joergr
** Added/modified getOFStringArray() implementation.
** Added support for XML output of DICOM objects.
**
** Revision 1.37  2001/11/16 15:55:02  meichel
** Adapted digital signature code to final text of supplement 41.
**
** Revision 1.36  2001/11/01 14:55:36  wilkens
** Added lots of comments.
**
** Revision 1.35  2001/09/25 17:19:49  meichel
** Adapted dcmdata to class OFCondition
**
** Revision 1.34  2001/06/01 15:49:03  meichel
** Updated copyright header
**
** Revision 1.33  2001/05/10 12:50:23  meichel
** Added protected createEmptyValue() method in class DcmElement.
**
** Revision 1.32  2000/11/07 16:56:19  meichel
** Initial release of dcmsign module for DICOM Digital Signatures
**
** Revision 1.31  2000/04/14 15:55:04  meichel
** Dcmdata library code now consistently uses ofConsole for error output.
**
** Revision 1.30  2000/03/08 16:26:34  meichel
** Updated copyright header.
**
** Revision 1.29  2000/03/03 14:05:32  meichel
** Implemented library support for redirecting error messages into memory
**   instead of printing them to stdout/stderr for GUI applications.
**
** Revision 1.28  2000/02/23 15:11:51  meichel
** Corrected macro for Borland C++ Builder 4 workaround.
**
** Revision 1.27  2000/02/01 10:12:06  meichel
** Avoiding to include <stdlib.h> as extern "C" on Borland C++ Builder 4,
**   workaround for bug in compiler header files.
**
** Revision 1.26  1999/03/31 09:25:26  meichel
** Updated copyright header in module dcmdata
**
** Revision 1.25  1999/03/22 15:46:24  meichel
** Printing explicit error message when DICOM file is too short.
**
** Revision 1.24  1998/11/12 16:48:15  meichel
** Implemented operator= for all classes derived from DcmObject.
**
** Revision 1.23  1998/07/15 15:51:55  joergr
** Removed several compiler warnings reported by gcc 2.8.1 with
** additional options, e.g. missing copy constructors and assignment
** operators, initialization of member variables in the body of a
** constructor instead of the member initialization list, hiding of
** methods by use of identical names, uninitialized member variables,
** missing const declaration of char pointers. Replaced tabs by spaces.
**
** Revision 1.22  1998/01/14 15:22:35  hewett
** Replaced a switch construct to use to the isaString method.
**
** Revision 1.21  1997/09/11 15:24:39  hewett
** Added a putOFStringArray method.
**
** Revision 1.20  1997/08/29 08:32:54  andreas
** - Added methods getOFString and getOFStringArray for all
**   string VRs. These methods are able to normalise the value, i. e.
**   to remove leading and trailing spaces. This will be done only if
**   it is described in the standard that these spaces are not relevant.
**   These methods do not test the strings for conformance, this means
**   especially that they do not delete spaces where they are not allowed!
**   getOFStringArray returns the string with all its parts separated by \
**   and getOFString returns only one value of the string.
**   CAUTION: Currently getString returns a string with trailing
**   spaces removed (if dcmEnableAutomaticInputDataCorrection == OFTrue) and
**   truncates the original string (since it is not copied!). If you rely on this
**   behaviour please change your application now.
**   Future changes will ensure that getString returns the original
**   string from the DICOM object (NULL terminated) inclusive padding.
**   Currently, if you call getOF... before calling getString without
**   normalisation, you can get the original string read from the DICOM object.
**
** Revision 1.19  1997/07/31 06:58:04  andreas
** new protected method swapValueField for DcmElement
**
** Revision 1.18  1997/07/24 13:10:51  andreas
** - Removed Warnings from SUN CC 2.0.1
**
** Revision 1.17  1997/07/21 07:57:57  andreas
** - New method DcmElement::detachValueField to give control over the
**   value field to the calling part (see dcelem.h)
** - Replace all boolean types (BOOLEAN, CTNBOOLEAN, DICOM_BOOL, BOOL)
**   with one unique boolean type OFBool.
**
** Revision 1.16  1997/07/07 07:46:19  andreas
** - Changed parameter type DcmTag & to DcmTagKey & in all search functions
**   in DcmItem, DcmSequenceOfItems, DcmDirectoryRecord and DcmObject
** - Enhanced (faster) byte swapping routine. swapIfNecessary moved from
**   a method in DcmObject to a general function.
**
** Revision 1.15  1997/07/03 15:09:57  andreas
** - removed debugging functions Bdebug() and Edebug() since
**   they write a static array and are not very useful at all.
**   Cdebug and Vdebug are merged since they have the same semantics.
**   The debugging functions in dcmdata changed their interfaces
**   (see dcmdata/include/dcdebug.h)
**
** Revision 1.14  1997/05/27 13:48:58  andreas
** - Add method canWriteXfer to class DcmObject and all derived classes.
**   This method checks whether it is possible to convert the original
**   transfer syntax to an new transfer syntax. The check is used in the
**   dcmconv utility to prohibit the change of a compressed transfer
**   syntax to a uncompressed.
**
** Revision 1.13  1997/05/16 08:23:53  andreas
** - Revised handling of GroupLength elements and support of
**   DataSetTrailingPadding elements. The enumeratio E_GrpLenEncoding
**   got additional enumeration values (for a description see dctypes.h).
**   addGroupLength and removeGroupLength methods are replaced by
**   computeGroupLengthAndPadding. To support Padding, the parameters of
**   element and sequence write functions changed.
** - Added a new method calcElementLength to calculate the length of an
**   element, item or sequence. For elements it returns the length of
**   tag, length field, vr field, and value length, for item and
**   sequences it returns the length of the whole item. sequence including
**   the Delimitation tag (if appropriate).  It can never return
**   UndefinedLength.
**
** Revision 1.12  1997/05/15 12:29:02  andreas
** - Bug fix for changing binary element values. If a binary existing element
**   value changed, byte order was somtimes wrong.
**
** Revision 1.11  1997/04/18 08:17:16  andreas
** - The put/get-methods for all VRs did not conform to the C++-Standard
**   draft. Some Compilers (e.g. SUN-C++ Compiler, Metroworks
**   CodeWarrier, etc.) create many warnings concerning the hiding of
**   overloaded get methods in all derived classes of DcmElement.
**   So the interface of all value representation classes in the
**   library are changed rapidly, e.g.
**   OFCondition get(Uint16 & value, const unsigned long pos);
**   becomes
**   OFCondition getUint16(Uint16 & value, const unsigned long pos);
**   All (retired) "returntype get(...)" methods are deleted.
**   For more information see dcmdata/include/dcelem.h
**
** Revision 1.10  1996/07/31 13:41:23  andreas
** *** empty log message ***
**
** Revision 1.9  1996/07/31 13:26:01  andreas
** -  Minor corrections: error code for swapping to or from byteorder unknown
**                       correct read of dataset in fileformat
**
** Revision 1.8  1996/07/29 17:14:26  andreas
** Faster Access with empty value fields
**
** Revision 1.7  1996/04/16 16:04:05  andreas
** - new put parameter DcmTagKey for DcmAttributeTag elements
** - better support for NULL element value
**
** Revision 1.6  1996/03/11 13:11:05  hewett
** Changed prototypes to allow get() and put() of char strings.
**
** Revision 1.5  1996/01/09 11:06:45  andreas
** New Support for Visual C++
** Correct problems with inconsistent const declarations
** Correct error in reading Item Delimitation Elements
**
** Revision 1.4  1996/01/05 14:00:24  andreas
** - add forgotten initialization for the byte order
**
** Revision 1.3  1996/01/05 13:27:36  andreas
** - changed to support new streaming facilities
** - unique read/write methods for file and block transfer
** - more cleanups
**
*/