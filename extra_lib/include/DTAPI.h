//*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#* DTAPI.h *#*#*#*#*#*#*#*#*#* (C) 2000-2014 DekTec
//
// DTAPI - C++ API for DekTec PCI/PCI-Express cards, USB-2 adapters and network devices
//
 
#ifndef __DTAPI_H
#define __DTAPI_H

// DTAPI version
#define DTAPI_VERSION_MAJOR        5
#define DTAPI_VERSION_MINOR        8
#define DTAPI_VERSION_BUGFIX       0
#define DTAPI_VERSION_BUILD        43

//-.-.-.-.-.-.-.-.-.-.-.-.- Additional Libraries to be Linked In -.-.-.-.-.-.-.-.-.-.-.-.-

#ifdef _WIN32

#ifndef _LIB  // Do not link libraries into DTAPI itself
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wbemuuid.lib")
#endif

#ifndef _DTAPI_DISABLE_AUTO_LINK
    // Are we using multi-threaded-DLL or static runtime libraries?
    #ifdef _DLL
        // Link with DLL runtime version (/MD)
        #ifdef _DEBUG
            #ifdef _WIN64
                #pragma comment(lib, "DTAPI64MDd.lib")  // Debug 64bit
                #pragma message("Automatically linking with DTAPI64MDd.lib")
            #else
                #pragma comment(lib, "DTAPIMDd.lib")    // Debug 32bit
                #pragma message("Automatically linking with DTAPIMDd.lib") 
            #endif
        #else
            #ifdef _WIN64
                #pragma comment(lib, "DTAPI64MD.lib")   // Release 64bit
                #pragma message("Automatically linking with DTAPI64MD.lib")
            #else
                #pragma comment(lib, "DTAPIMD.lib")     // Release 32bit
                #pragma message("Automatically linking with DTAPIMD.lib") 
            #endif
        #endif
    #else
        // Link to static runtime version (/MT)
        #ifdef _DEBUG
            #ifdef _WIN64
                #pragma comment(lib, "DTAPI64MTd.lib")  // Debug 64bit
                #pragma message("Automatically linking with DTAPI64MTd.lib")
            #else
                #pragma comment(lib, "DTAPIMTd.lib")    // Debug 32bit
                #pragma message("Automatically linking with DTAPIMTd.lib") 
            #endif
        #else
            #ifdef _WIN64
                #pragma comment(lib, "DTAPI64MT.lib")   // Release 64bit
                #pragma message("Automatically linking with DTAPI64MT.lib")
            #else
                #pragma comment(lib, "DTAPIMT.lib")     // Release 32bit
                #pragma message("Automatically linking with DTAPIMT.lib") 
            #endif
        #endif
    #endif
#endif
#endif    // #ifdef _WIN32

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Includes -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-

#ifndef _WIN32
    // Linux specific includes: For NULL type definition
    #include </usr/include/linux/stddef.h>
    // For intptr_t
    #include <stdint.h>
#endif

// STL includes
#include <list>
#include <map>
#include <string>
#include <vector>
#include <limits>

// When creating a DLL under Windows, disable warnings related to exporting STL
// instantiations in classes.
// See also: http://support.microsoft.com/kb/q168958/
#ifdef _MSC_VER
    #pragma warning(disable: 4251)
#endif

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Support Types -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-

#ifndef _WIN32
    typedef signed long long __int64;
#endif

// All DTAPI code lives in namespace Dtapi
namespace Dtapi
{

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Forward declarations -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-

class AdvDemod;
class DtaPlusDevice;
class DtDvbT2Pars;
class DteDevice;
class DtSdiUtility;
class FrameBufImpl;
class IDevice;
class IDtDemodEvent;
class InpChannel;
class IXpMutex;
class MplpHelper;
class OutpChannel;
class SdiMatrixImpl;

struct DtDabEnsembleInfo;
struct DtDabEtiStreamSelPars;
struct DtDabStreamSelPars;
struct DtDemodParsAtsc;
struct DtDemodParsDab;
struct DtDemodParsDvbC2;
struct DtDemodParsDvbS;
struct DtDemodParsDvbS2;
struct DtDemodParsDvbT;
struct DtDemodParsDvbT2;
struct DtDemodParsIq;
struct DtDemodParsIq2131;
struct DtDemodParsIsdbt;
struct DtDemodParsQam;
struct DtDemodLdpcStats;
struct DtDemodMaLayerData;
struct DtDemodMaLayerStats;
struct DtDemodPlpBlocks;
struct DtDvbC2DemodL1Part2Data;
struct DtDvbC2DemodL1PlpSigData;
struct DtDvbC2ModStatus;
struct DtDvbC2Pars;
struct DtDvbC2PlpPars;
struct DtDvbC2StreamSelPars;
struct DtDvbC2XFecFrameHeader;
struct DtDvbS2ModStatus;
struct DtDvbS2ModCod;
struct DtDvbS2Pars;
struct DtDemodParsDvbS2Adv;
struct DtDvbTStreamSelPars;
struct DtDvbTTpsInfo;
struct DtDvbT2DemodL1Data;
struct DtDvbT2ModStatus;
struct DtDvbT2StreamSelPars;
struct DtIsdbtStreamSelPars;
struct DtIsdbTmmPars;
struct DtPar;
struct DtPlpInpPars;
struct DtStatistic;
struct DtT2MiStreamSelPars;
struct DtVirtualOutData;
struct DtRsDecStats;
struct DtVitDecStats;

//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
//=+=+=+=+=+=+=+=+=+=+=+=+ DTAPI HELPER CLASSES AND HELPER TYPES +=+=+=+=+=+=+=+=+=+=+=+=+
//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DTAPI_RESULT -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-

// Type returned by most API calls
typedef unsigned int  DTAPI_RESULT;

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtExc -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Exception object thrown from API calls (if API call throws an exception)
//
class DtExc
{
public:
    const DTAPI_RESULT  m_Error;    // DTAPI result code (i.e. reason of exception)
    
public:
    DtExc(DTAPI_RESULT e) : m_Error(e) {}
    virtual ~DtExc() {}
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtBufferInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
struct DtBufferInfo
{
    int  m_VidStd;                  // Video standard 
    int  m_NumColumns;              // Number of columns 
    __int64  m_NumReceived;         // Number of frames received
    __int64  m_NumDropped;          // Number of frames dropped
    __int64  m_NumTransmitted;      // Number of frames transmitted
    __int64  m_NumDuplicated;       // Number of frames duplicated
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtCaps - Capabilities -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Scalable type for storing (combinations of) capability flags.
// It can be used with bitwise operators for testing and setting of capabilities.
//
class DtCaps 
{
    // Number of 64-bit integers used to represent a capability
    static const int  DTCAPS_SIZE = 3;

public:
    DtCaps();
    DtCaps(int BitNr);
    DtCaps(__int64 F1, __int64 F2, __int64 F3);

public:
    int  GetCapIndex() const;
    std::string  ToString() const;
    DtCaps  operator & (const DtCaps& Caps) const;
    DtCaps&  operator &= (const DtCaps& Caps);
    DtCaps  operator | (const DtCaps& Caps) const;
    DtCaps&  operator |= (const DtCaps& Caps);
    bool  operator == (const DtCaps& Caps) const;
    bool  operator == (const int Zero) const;
    bool  operator != (const DtCaps& Caps) const;
    bool  operator != (const int Zero) const;
    __int64  operator [] (const int n) const;
    DtCaps  operator ~ () const;
    // Implementation data
private:
    __int64  m_Flags[DTCAPS_SIZE];
};

// Capabilities
#define DTAPI_CAP_EMPTY        Dtapi::DtCaps()   // DtCaps without any capability flags

// Capability group 3GLVL - 3G-SDI level config
#define DTAPI_CAP_3GLVLA      Dtapi::DtCaps(0)   // 3G-SDI level A
#define DTAPI_CAP_3GLVLB      Dtapi::DtCaps(1)   // 3G-SDI level B

// Capability group APPS - Applications
#define DTAPI_CAP_C2X         Dtapi::DtCaps(2)   // C2Xpert
#define DTAPI_CAP_DP          Dtapi::DtCaps(3)   // DtGrabber+ and DtTV
#define DTAPI_CAP_DTTV        Dtapi::DtCaps(4)   // DtTV
#define DTAPI_CAP_E           Dtapi::DtCaps(5)   // DtEncode
#define DTAPI_CAP_J           Dtapi::DtCaps(6)   // DtJitter
#define DTAPI_CAP_MR          Dtapi::DtCaps(7)   // MuxXpert runtime
#define DTAPI_CAP_MS          Dtapi::DtCaps(8)   // MuxXpert SDK
#define DTAPI_CAP_MX          Dtapi::DtCaps(9)   // MuxXpert
#define DTAPI_CAP_RC          Dtapi::DtCaps(10)  // StreamXpress remote control
#define DTAPI_CAP_RX          Dtapi::DtCaps(11)  // RFXpert
#define DTAPI_CAP_SP          Dtapi::DtCaps(12)  // StreamXpress stream player
#define DTAPI_CAP_SX          Dtapi::DtCaps(13)  // StreamXpert analyzer
#define DTAPI_CAP_SXDGL       Dtapi::DtCaps(14)  // StreamXpert analyzer (dongled)
#define DTAPI_CAP_SY          Dtapi::DtCaps(15)  // SdEye
#define DTAPI_CAP_XP          Dtapi::DtCaps(16)  // Xpect
#define DTAPI_CAP_T2X         Dtapi::DtCaps(17)  // T2Xpert

// Capability group BOOLIO - Boolean I/O capabilities
#define DTAPI_CAP_BW          Dtapi::DtCaps(18)  // Isochronous bandwidth allocated
#define DTAPI_CAP_FAILSAFE    Dtapi::DtCaps(19)  // A fail-over relay is available
#define DTAPI_CAP_FRACMODE    Dtapi::DtCaps(20)  // Fractional mode is supported
#define DTAPI_CAP_GENLOCKED   Dtapi::DtCaps(21)  // Locked to a genlock reference
#define DTAPI_CAP_GENREF      Dtapi::DtCaps(22)  // Genlock reference input
#define DTAPI_CAP_SWS2APSK    Dtapi::DtCaps(23)  // DVB-S2 APSK mode

// Capability group DEMODPROPS - Demodulation properties
#define DTAPI_CAP_ANTPWR      Dtapi::DtCaps(24)  // Antenna power
#define DTAPI_CAP_LNB         Dtapi::DtCaps(25)  // LNB
#define DTAPI_CAP_RX_ADV      Dtapi::DtCaps(26)  // Advanced demodulation

// Capability group FREQBAND - Frequency band
#define DTAPI_CAP_LBAND       Dtapi::DtCaps(27)  // L-band 950-2150MHz
#define DTAPI_CAP_VHF         Dtapi::DtCaps(28)  // VHF-band 47-470MHz
#define DTAPI_CAP_UHF         Dtapi::DtCaps(29)  // UHF-band 400-862MHz

// Capability group IODIR - I/O direction
#define DTAPI_CAP_DISABLED    Dtapi::DtCaps(30)  // Port is disabled
#define DTAPI_CAP_INPUT       Dtapi::DtCaps(31)  // Uni-directional input
#define DTAPI_CAP_OUTPUT      Dtapi::DtCaps(32)  // Uni-directional output

// Subcapabilities of IODIR, DTAPI_CAP_INPUT
#define DTAPI_CAP_SHAREDANT   Dtapi::DtCaps(33)  // Get antenna signal from another port

// Subcapabilities of IODIR, DTAPI_CAP_OUTPUT
#define DTAPI_CAP_DBLBUF      Dtapi::DtCaps(34)  // Double buffered output
#define DTAPI_CAP_LOOPS2L3    Dtapi::DtCaps(35)  // Loop-through of DVB-S2 in L3-frames
#define DTAPI_CAP_LOOPS2TS    Dtapi::DtCaps(36)  // Loop-through of an DVB-S(2) input
#define DTAPI_CAP_LOOPTHR     Dtapi::DtCaps(37)  // Loop-through of another input

// Capability group IOPROPS - Miscellaneous I/O properties
#define DTAPI_CAP_ASIPOL      Dtapi::DtCaps(38)  // ASI output signal can be inverted
#define DTAPI_CAP_HUFFMAN     Dtapi::DtCaps(39)  // Huffman coding for SDI
#define DTAPI_CAP_IPPAIR      Dtapi::DtCaps(40)  // Network port supports failover
#define DTAPI_CAP_L3MODE      Dtapi::DtCaps(41)  // L3-frame mode
#define DTAPI_CAP_MATRIX      Dtapi::DtCaps(42)  // Matrix API support
#define DTAPI_CAP_RAWASI      Dtapi::DtCaps(43)  // Raw ASI
#define DTAPI_CAP_SDI10BNBO   Dtapi::DtCaps(44)  // 10-bit network byte order
#define DTAPI_CAP_SDITIME     Dtapi::DtCaps(45)  // SDI timestamping
#define DTAPI_CAP_TIMESTAMP64 Dtapi::DtCaps(46)  // 64-bit timestamping
#define DTAPI_CAP_TRPMODE     Dtapi::DtCaps(47)  // Transparent mode
#define DTAPI_CAP_TS          Dtapi::DtCaps(48)  // MPEG-2 transport stream
#define DTAPI_CAP_TXONTIME    Dtapi::DtCaps(49)  // Transmit on timestamp

// Capability group IOSTD - I/O standard
#define DTAPI_CAP_3GSDI       Dtapi::DtCaps(50)  // 3G-SDI
#define DTAPI_CAP_ASI         Dtapi::DtCaps(51)  // DVB-ASI transport stream
#define DTAPI_CAP_DEMOD       Dtapi::DtCaps(52)  // Demodulation
#define DTAPI_CAP_HDSDI       Dtapi::DtCaps(53)  // HD-SDI
#define DTAPI_CAP_IFADC       Dtapi::DtCaps(54)  // IF A/D converter
#define DTAPI_CAP_IP          Dtapi::DtCaps(55)  // Transport stream over IP
#define DTAPI_CAP_MOD         Dtapi::DtCaps(56)  // Modulator output
#define DTAPI_CAP_RS422       Dtapi::DtCaps(57)  // RS422 port
#define DTAPI_CAP_SDI         Dtapi::DtCaps(58)  // SD-SDI
#define DTAPI_CAP_SPI         Dtapi::DtCaps(59)  // DVB-SPI transport stream
#define DTAPI_CAP_SPISDI      Dtapi::DtCaps(60)  // SD-SDI on a parallel port

// Subcapabilities of IOSTD, DTAPI_CAP_3GSDI
#define DTAPI_CAP_1080P50     Dtapi::DtCaps(61)  // 1080p/50
#define DTAPI_CAP_1080P59_94  Dtapi::DtCaps(62)  // 1080p/59.94
#define DTAPI_CAP_1080P60     Dtapi::DtCaps(63)  // 1080p/60

// Subcapabilities of IOSTD, DTAPI_CAP_HDSDI
#define DTAPI_CAP_1080I50     Dtapi::DtCaps(64)  // 1080i/50
#define DTAPI_CAP_1080I59_94  Dtapi::DtCaps(65)  // 1080i/59.94
#define DTAPI_CAP_1080I60     Dtapi::DtCaps(66)  // 1080i/60
#define DTAPI_CAP_1080P23_98  Dtapi::DtCaps(67)  // 1080p/23.98
#define DTAPI_CAP_1080P24     Dtapi::DtCaps(68)  // 1080p/24
#define DTAPI_CAP_1080P25     Dtapi::DtCaps(69)  // 1080p/25
#define DTAPI_CAP_1080P29_97  Dtapi::DtCaps(70)  // 1080p/29.97
#define DTAPI_CAP_1080P30     Dtapi::DtCaps(71)  // 1080p/30
#define DTAPI_CAP_720P23_98   Dtapi::DtCaps(72)  // 720p/23.98
#define DTAPI_CAP_720P24      Dtapi::DtCaps(73)  // 720p/24
#define DTAPI_CAP_720P25      Dtapi::DtCaps(74)  // 720p/25
#define DTAPI_CAP_720P29_97   Dtapi::DtCaps(75)  // 720p/29.97
#define DTAPI_CAP_720P30      Dtapi::DtCaps(76)  // 720p/30
#define DTAPI_CAP_720P50      Dtapi::DtCaps(77)  // 720p/50
#define DTAPI_CAP_720P59_94   Dtapi::DtCaps(78)  // 720p/59.94
#define DTAPI_CAP_720P60      Dtapi::DtCaps(79)  // 720p/60

// Subcapabilities of IOSTD, DTAPI_CAP_SDI
#define DTAPI_CAP_525I59_94   Dtapi::DtCaps(80)  // 525i/59.94
#define DTAPI_CAP_625I50      Dtapi::DtCaps(81)  // 625i/50

// Subcapabilities of IOSTD, DTAPI_CAP_SPISDI
#define DTAPI_CAP_SPI525I59_94 Dtapi::DtCaps(82) // SPI 525i/59.94
#define DTAPI_CAP_SPI625I50   Dtapi::DtCaps(83)  // SPI 625i/50

// Capability group MODSTD - Modulation standards
#define DTAPI_CAP_TX_ATSC     Dtapi::DtCaps(84)  // ATSC 8-VSB modulation
#define DTAPI_CAP_TX_CMMB     Dtapi::DtCaps(85)  // CMMB modulation
#define DTAPI_CAP_TX_DAB      Dtapi::DtCaps(86)  // DAB modulation
#define DTAPI_CAP_TX_DTMB     Dtapi::DtCaps(87)  // DTMB modulation
#define DTAPI_CAP_TX_DVBC2    Dtapi::DtCaps(88)  // DVB-C2 modulation
#define DTAPI_CAP_TX_DVBS     Dtapi::DtCaps(89)  // DVB-S modulation
#define DTAPI_CAP_TX_DVBS2    Dtapi::DtCaps(90)  // DVB-S2 modulation
#define DTAPI_CAP_TX_DVBS2X   Dtapi::DtCaps(91)  // DVB-S2X modulation
#define DTAPI_CAP_TX_DVBT     Dtapi::DtCaps(92)  // DVB-T modulation
#define DTAPI_CAP_TX_DVBT2    Dtapi::DtCaps(93)  // DVB-T2 modulation
#define DTAPI_CAP_TX_GOLD     Dtapi::DtCaps(94)  // GOLD for modulators
#define DTAPI_CAP_TX_IQ       Dtapi::DtCaps(95)  // I/Q sample modulation
#define DTAPI_CAP_TX_ISDBS    Dtapi::DtCaps(96)  // ISDB-S modulation
#define DTAPI_CAP_TX_ISDBT    Dtapi::DtCaps(97)  // ISDB-T modulation
#define DTAPI_CAP_TX_ISDBTMM  Dtapi::DtCaps(98)  // ISDB-Tmm modulation
#define DTAPI_CAP_TX_MH       Dtapi::DtCaps(99)  // ATSC-MH modulation
#define DTAPI_CAP_TX_QAMA     Dtapi::DtCaps(100) // QAM-A modulation
#define DTAPI_CAP_TX_QAMB     Dtapi::DtCaps(101) // QAM-B modulation
#define DTAPI_CAP_TX_QAMC     Dtapi::DtCaps(102) // QAM-C modulation
#define DTAPI_CAP_TX_T2MI     Dtapi::DtCaps(103) // T2MI transmission
#define DTAPI_CAP_TX_T2SPLP   Dtapi::DtCaps(104) // DVB-T2 single PLP modulation

// Capability group MODPROPS - Modulation properties
#define DTAPI_CAP_ADJLVL      Dtapi::DtCaps(105) // Adjustable output level
#define DTAPI_CAP_CM          Dtapi::DtCaps(106) // Channel simulation
#define DTAPI_CAP_CW          Dtapi::DtCaps(107) // Continuous wave
#define DTAPI_CAP_DIGIQ       Dtapi::DtCaps(108) // Digital I/Q sample output
#define DTAPI_CAP_IF          Dtapi::DtCaps(109) // IF output
#define DTAPI_CAP_MUTE        Dtapi::DtCaps(110) // Mute RF output signal
#define DTAPI_CAP_ROLLOFF     Dtapi::DtCaps(111) // Adjustable roll-off factor
#define DTAPI_CAP_S2APSK      Dtapi::DtCaps(112) // DVB-S2 16-APSK/32-APSK
#define DTAPI_CAP_SNR         Dtapi::DtCaps(113) // AWGN insertion
#define DTAPI_CAP_TX_16MHZ    Dtapi::DtCaps(114) // 16MHz bandwidth mode

// Capability group RFCLKSEL - RF clock source selection
#define DTAPI_CAP_RFCLKEXT    Dtapi::DtCaps(115) // External RF clock input
#define DTAPI_CAP_RFCLKINT    Dtapi::DtCaps(116) // Internal RF clock reference

// Capability group RXSTD - Receiver standards
#define DTAPI_CAP_RX_ATSC     Dtapi::DtCaps(117) // ATSC 8-VSB reception
#define DTAPI_CAP_RX_CMMB     Dtapi::DtCaps(118) // CMMB reception
#define DTAPI_CAP_RX_DAB      Dtapi::DtCaps(119) // DAB reception
#define DTAPI_CAP_RX_DTMB     Dtapi::DtCaps(120) // DTMB reception
#define DTAPI_CAP_RX_DVBC2    Dtapi::DtCaps(121) // DVB-C2 reception
#define DTAPI_CAP_RX_DVBS     Dtapi::DtCaps(122) // DVB-S reception
#define DTAPI_CAP_RX_DVBS2    Dtapi::DtCaps(123) // DVB-S2 reception
#define DTAPI_CAP_RX_DVBT     Dtapi::DtCaps(124) // DVB-T reception
#define DTAPI_CAP_RX_DVBT2    Dtapi::DtCaps(125) // DVB-T2 reception
#define DTAPI_CAP_RX_GOLD     Dtapi::DtCaps(126) // GOLD for receivers
#define DTAPI_CAP_RX_IQ       Dtapi::DtCaps(127) // I/Q sample reception
#define DTAPI_CAP_RX_ISDBS    Dtapi::DtCaps(128) // ISDB-S reception
#define DTAPI_CAP_RX_ISDBT    Dtapi::DtCaps(129) // ISDB-T reception
#define DTAPI_CAP_RX_MH       Dtapi::DtCaps(130) // ATSC-MH reception
#define DTAPI_CAP_RX_QAMA     Dtapi::DtCaps(131) // QAM-A reception
#define DTAPI_CAP_RX_QAMB     Dtapi::DtCaps(132) // QAM-B reception
#define DTAPI_CAP_RX_QAMC     Dtapi::DtCaps(133) // QAM-C reception
#define DTAPI_CAP_RX_T2MI     Dtapi::DtCaps(134) // T2MI reception

// Capability group SPICLKSEL - Parallel port clock source selection
#define DTAPI_CAP_SPICLKEXT   Dtapi::DtCaps(135) // External clock input
#define DTAPI_CAP_SPICLKINT   Dtapi::DtCaps(136) // Internal clock reference

// Capability group SPIMODE - Parallel port mode
#define DTAPI_CAP_SPIFIXEDCLK Dtapi::DtCaps(137) // SPI fixed clock with valid signal
#define DTAPI_CAP_SPIDVBMODE  Dtapi::DtCaps(138) // SPI DVB mode
#define DTAPI_CAP_SPISER8B    Dtapi::DtCaps(139) // SPI serial 8-bit mode
#define DTAPI_CAP_SPISER10B   Dtapi::DtCaps(140) // SPI serial 10-bit mode

// Capability group SPISTD - Parallel port I/O standard
#define DTAPI_CAP_SPILVDS1    Dtapi::DtCaps(141) // LVDS1
#define DTAPI_CAP_SPILVDS2    Dtapi::DtCaps(142) // LVDS2
#define DTAPI_CAP_SPILVTTL    Dtapi::DtCaps(143) // LVTTL

// Capability group TSRATESEL - Transport-stream rate selection
#define DTAPI_CAP_EXTTSRATE   Dtapi::DtCaps(144) // External TS rate clock input
#define DTAPI_CAP_EXTRATIO    Dtapi::DtCaps(145) // External TS rate clock with ratio
#define DTAPI_CAP_INTTSRATE   Dtapi::DtCaps(146) // Internal TS rate clock reference
#define DTAPI_CAP_LOCK2INP    Dtapi::DtCaps(147) // Lock TS rate to input port

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtCmmbPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// CMMB Modulation Parameters
//
struct DtCmmbPars
{
    int  m_Bandwidth;               // CMMB Bandwitdh
    int  m_TsRate;                  // CMMB TS rate in bps
    int  m_TsPid;                   // PID on which the CMMB stream is found
    int  m_AreaId;                  // Area ID (0..127)
    int  m_TxId;                    // Transmitter ID (128..255)
    
public:
    DtCmmbPars();
    DTAPI_RESULT  RetrieveTsRateFromTs(char* pBuffer, int NumBytes);
    bool  operator == (DtCmmbPars& Rhs);
    bool  operator != (DtCmmbPars& Rhs);
};

// DtOutpChannel::SetModControl - Bandwidth
#define DTAPI_CMMB_BW_2MHZ          0x00000000
#define DTAPI_CMMB_BW_8MHZ          0x00000001

//-.-.-.-.-.-.-.-.-.-.-.- DtCmPars - Channel Modelling Parameters -.-.-.-.-.-.-.-.-.-.-.-.

// Maximum number of fading paths
#define DTAPI_CM_MAX_PATHS          32

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtCmPath -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure describes the fading parameters for a single path in a
// multi-path simulation.
//
struct DtCmPath
{
    enum Type
    {
        CONSTANT_DELAY,             // Constant delay/phase
        CONSTANT_DOPPLER,           // Constant frequency shift
        RAYLEIGH_JAKES,             // Rayleigh fading with Jakes power spectral density
                                    // (mobile path model)
        RAYLEIGH_GAUSSIAN           // Rayleigh fading with Gaussian power spectral
                                    // density (ionospheric path model)
    };
    Type  m_Type;                   // Type of path fading
    double  m_Attenuation;          // Attenuation in dB
    double  m_Delay;                // Delay in us
    double  m_Phase;                // Phase shift in degrees for CONSTANT_DELAY paths
    double  m_Doppler;              // Doppler frequency in Hz

    // Constructor - Gives parameters a default value
    DtCmPath() :
        m_Type(CONSTANT_DELAY),
        m_Attenuation(0.0), m_Delay(0.0), m_Phase(0.0), m_Doppler(0.0)
    {}
    // Operators
    bool  operator == (DtCmPath& Rhs);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtCmPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure describes channel-modeling parameters. It\92s used to simulate the 
// transmission distortions that may occur in the channel between a transmitter and
// a receiver.
//
struct DtCmPars
{
    bool  m_EnableAwgn;             // Enable white Gaussian noise (AWGN) injection
    double  m_Snr;                  // Signal-to-noise ratio in dB
    bool  m_EnablePaths;            // Enable multi-path simulation
    std::vector<DtCmPath>  m_Paths; // Parameters per path

    // Constructor and operators
    DtCmPars();
    bool  operator == (DtCmPars&);
    bool  operator != (DtCmPars& Rhs) { return !(*this == Rhs); }
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtConstelPoint -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure describes a constellation point in a receiver constellation diagram
//
struct DtConstelPoint
{
    int  m_X, m_Y;                  // X and Y coordinates
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- IDtDemodEvent -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Callback event interface for demodulators
//
class IDtDemodEvent
{
public:
    virtual void  TuningFreqHasChanged(__int64 OldFreqHz, __int64 NewFreqHz) {}
    virtual void  TuningParsHaveChanged(
                                __int64 OldFreqHz, int  OldModType, int  OldParXtra[3],
                                __int64 NewFreqHz, int  NewModType, int  NewParXtra[3]) {}
};


// Maximum number of IpV6 addresses per interface
#define MAX_IPV6_ADDR               3

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDeviceDesc -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure describes a DekTec device
//
struct DtDeviceDesc
{
    int  m_Category;                // Device category (DTAPI_CAT_XXX)
    __int64  m_Serial;              // Unique serial number of the device
    int  m_PciBusNumber;            // PCI-bus number
    int  m_SlotNumber;              // PCI-slot number
    int  m_UsbAddress;              // USB address
    int  m_TypeNumber;              // Device type number
    int  m_SubType;                 // Device subtype (0=none, 1=A, ...)
    int  m_DeviceId;                // Device ID
    int  m_VendorId;                // Vendor ID
    int  m_SubsystemId;             // Subsystem ID
    int  m_SubVendorId;             // Subsystem Vendor ID
    int  m_NumHwFuncs;              // Number of hardware funtions hosted by device
    int  m_HardwareRevision;        // Hardware revision (e.g. 302 = 3.2)
    int  m_FirmwareVersion;         // Firmware version
    int  m_FirmwareVariant;         // Firmware variant
    int  m_NumDtInpChan;            // Number of input channels (max)
    int  m_NumDtOutpChan;           // Number of output channels (max)
    int  m_NumPorts;                // Number of physical ports
    unsigned char  m_Ip[4];         // IP address (only valid for DTE-31xx devices)
    unsigned char  m_IpV6[MAX_IPV6_ADDR][16];
                                    // IP address (only valid for DTE-31xx devices)
    unsigned char  m_MacAddr[6];    // MAC address (only valid for DTE-31xx devices)
    int  m_PcieNumLanes;            // Number of allocated PCIe lanes
    int  m_PcieMaxLanes;            // Maximum number of PCIe lanes
    int  m_PcieLinkSpeed;           // Current PCIe link speed (GEN1, 2 or 3)
    int  m_PcieMaxSpeed;            // Maximum PCIe link speed (GEN1, 2 or 3)
};

// Device categories
#define DTAPI_CAT_PCI               0            // PCI or PCI-Express device
#define DTAPI_CAT_USB               1            // USB-2 or USB-3 device
#define DTAPI_CAT_NW                2            // Network device
#define DTAPI_CAT_IP                3            // Network appliance: DTE-31xx
#define DTAPI_CAT_NIC               4            // Non-DekTec network card
#define DTAPI_CAT_NWAP              5            // Network Advanced Protocol(VLAN device)

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDtaPlusDeviceDesc -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure describes a DekTec DTA-plus device
//
struct DtDtaPlusDeviceDesc
{
    __int64  m_Serial;              // Unique serial number of the device
    std::string  m_DevicePath;      // Path of file to open to interface with the device
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtEventArgs -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Details for a specific event
//
struct DtEventArgs
{
    DtEventArgs() : m_Value1(0), m_Value2(0), m_pContext(0) {};

    int  m_HwCat;                   // Hardware category: DTAPI_CAT_XXX
    __int64  m_Serial;              // Serial number of device causing event
    int  m_Value1;                  // Event value #1
    int  m_Value2;                  // Event value #2
    void*  m_pContext;              // Context-specific pointer
};

// Event call back function
typedef void (*pDtEventCallback)(int Event, const DtEventArgs* pArgs);

// Global events
#define DT_EVENT_TYPE_ADD           0x00000001
#define DT_EVENT_TYPE_REMOVE        0x00000002
// Device events
#define DT_EVENT_TYPE_POWER         0x00000004
#define DT_EVENT_TYPE_GENLOCK       0x00000008
#define DT_EVENT_TYPE_TEST          0x80000000
// Network events
#define DT_EVENT_IP_CHANGED         0x01000000
#define DT_EVENT_ADMINST_CHANGED    0x02000000

#define DT_EVENT_TYPE_ALL           0xFFFFFFFF

// Event values
#define DT_EVENT_VALUE1_POWER_UP    1
#define DT_EVENT_VALUE1_POWER_DOWN  2
#define DT_EVENT_VALUE1_NO_LOCK     1
#define DT_EVENT_VALUE1_LOCKED      2
#define DT_EVENT_VALUE2_XXX         1

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFiltCoeff -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// A single FIR filter coefficient
//
struct DtFiltCoeff
{
    int  m_TapIdx;                  // Tap number
    double  m_Coeff;                // FIR coefficient
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFilterPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Filter coefficients for use in a FIR filter
//
struct DtFilterPars
{
    std::vector<DtFiltCoeff>  m_FiltCoeffs;
};

// Maximum number of filter coefficients
#define DTAPI_MAX_NUM_COEFFS   64

#define DTAPI_FRAME_STATUS_OK                0
#define DTAPI_FRAME_STATUS_ERR_NO_SIGNAL     1
#define DTAPI_FRAME_STATUS_ERR_STD_MISMATCH  2

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
struct DtFrameInfo
{
    int  m_VidStd;                  // Video standard 
    __int64  m_Timestamp;           // 64-bit timestamp
    __int64  m_FrameNumber;         // 64-bit frame number
    __int64  m_Rp188;               // RP188 timestamp
    int  m_RxMode;                  // RX mode at the time this frame was received
    int  m_Status;                  // One of DTAPI_FRAME_STATUS_*
};

//-.-.-.-.-.-.-.-.-.-.- DtHwFuncDesc - Hardware Function Descriptor -.-.-.-.-.-.-.-.-.-.-.
//
// Structure describing a hardware function
//
struct DtHwFuncDesc
{
    DtDeviceDesc  m_DvcDesc;        // Device descriptor
    int  m_ChanType;                // Channel type (OR-able)
    DtCaps  m_Flags;                // Capability flags (OR-able)
    int  m_IndexOnDvc;              // Relative index of hardware function
    int  m_Port;                    // Physical port number
    unsigned char  m_Ip[4];         // IP V4 address (TS-over-IP functions only)
    unsigned char  m_IpV6[MAX_IPV6_ADDR][16];
                                    // IP V6 address (TS-over-IP functions only)
    unsigned char  m_MacAddr[6];    // MAC address (TS-over-IP functions only)
};

// Hardware Function - Channel types
// For IP hardware functions, both DTAPI_CHAN_INPUT and DTAPI_CHAN_OUTPUT are set
#define DTAPI_CHAN_DISABLED         0            // Channel is disabled
#define DTAPI_CHAN_INPUT            1            // Input channel
#define DTAPI_CHAN_OUTPUT           2            // Output channel
#define DTAPI_CHAN_DBLBUF           4            // Double-buffered copy of an output
#define DTAPI_CHAN_LOOPTHR          8            // Loop-through copy of another port

//.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIoConfig - I/O Configuration -.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Stores the I/O configuration parameters for one I/O port configuration
//
struct DtIoConfig
{
    int  m_Port;                    // Port number
    int  m_Group;                   // Config group, linked to I/O capability groups
    int  m_Value;                   // Config value, linked to I/O capabilities
    int  m_SubValue;                // Config sub value, linked to I/O sub capabilities
    __int64  m_ParXtra[2];          // Extra parameters, e.g. source port number

    // Constructor - Gives parameters a default value
    DtIoConfig()
    {
        m_Port = -1;
        m_Group = -1;
        m_Value = m_SubValue = -1;
        m_ParXtra[0] = m_ParXtra[1] = -1;
    }
    DtIoConfig(int  Port, int  Group)
    {
        m_Port = Port;
        m_Group = Group;
        m_Value = m_SubValue = -1;
        m_ParXtra[0] = m_ParXtra[1] = -1;
    }
};

// I/O configuration groups
#define DTAPI_IOCONFIG_3GLVL       0             // 3G-SDI level config
#define DTAPI_IOCONFIG_IODIR       1             // I/O direction
#define DTAPI_IOCONFIG_IOSTD       2             // I/O standard
#define DTAPI_IOCONFIG_RFCLKSEL    3             // RF clock source selection
#define DTAPI_IOCONFIG_SPICLKSEL   4             // Parallel port clock source selection
#define DTAPI_IOCONFIG_SPIMODE     5             // Parallel port mode
#define DTAPI_IOCONFIG_SPISTD      6             // Parallel port I/O standard
#define DTAPI_IOCONFIG_TSRATESEL   7             // Transport-stream rate selection

// I/O configuration groups - Boolean I/O
#define DTAPI_IOCONFIG_BW          8             // Isochronous bandwidth allocated
#define DTAPI_IOCONFIG_FAILSAFE    9             // A fail-over relay is available
#define DTAPI_IOCONFIG_FRACMODE    10            // Fractional mode is supported
#define DTAPI_IOCONFIG_GENLOCKED   11            // Locked to a genlock reference
#define DTAPI_IOCONFIG_GENREF      12            // Genlock reference input
#define DTAPI_IOCONFIG_SWS2APSK    13            // DVB-S2 APSK mode

// Values for group IO_CONFIG_3GLVL (3G-SDI level config)
#define DTAPI_IOCONFIG_3GLVLA      14            // 3G-SDI level A
#define DTAPI_IOCONFIG_3GLVLB      15            // 3G-SDI level B

// Values for boolean I/O configuration options
#define DTAPI_IOCONFIG_TRUE        16            // Turn I/O capability on
#define DTAPI_IOCONFIG_FALSE       17            // Turn I/O capability off

// Values for group IO_CONFIG_IODIR (I/O direction)
#define DTAPI_IOCONFIG_DISABLED    18            // Port is disabled
#define DTAPI_IOCONFIG_INPUT       19            // Uni-directional input
#define DTAPI_IOCONFIG_OUTPUT      20            // Uni-directional output

// SubValues for group DTAPI_IOCONFIG_IODIR, value DTAPI_IOCONFIG_INPUT
#define DTAPI_IOCONFIG_SHAREDANT   21            // Get antenna signal from another port

// SubValues for group DTAPI_IOCONFIG_IODIR, value DTAPI_IOCONFIG_OUTPUT
#define DTAPI_IOCONFIG_DBLBUF      22            // Double buffered output
#define DTAPI_IOCONFIG_LOOPS2L3    23            // Loop-through of DVB-S2 in L3-frames
#define DTAPI_IOCONFIG_LOOPS2TS    24            // Loop-through of an DVB-S(2) input
#define DTAPI_IOCONFIG_LOOPTHR     25            // Loop-through of another input

// Values for group IO_CONFIG_IOSTD (I/O standard)
#define DTAPI_IOCONFIG_3GSDI       26            // 3G-SDI
#define DTAPI_IOCONFIG_ASI         27            // DVB-ASI transport stream
#define DTAPI_IOCONFIG_DEMOD       28            // Demodulation
#define DTAPI_IOCONFIG_HDSDI       29            // HD-SDI
#define DTAPI_IOCONFIG_IFADC       30            // IF A/D converter
#define DTAPI_IOCONFIG_IP          31            // Transport stream over IP
#define DTAPI_IOCONFIG_MOD         32            // Modulator output
#define DTAPI_IOCONFIG_RS422       33            // RS422 port
#define DTAPI_IOCONFIG_SDI         34            // SD-SDI
#define DTAPI_IOCONFIG_SPI         35            // DVB-SPI transport stream
#define DTAPI_IOCONFIG_SPISDI      36            // SD-SDI on a parallel port

// SubValues for group DTAPI_IOCONFIG_IOSTD, value DTAPI_IOCONFIG_3GSDI
#define DTAPI_IOCONFIG_1080P50     37            // 1080p/50
#define DTAPI_IOCONFIG_1080P59_94  38            // 1080p/59.94
#define DTAPI_IOCONFIG_1080P60     39            // 1080p/60

// SubValues for group DTAPI_IOCONFIG_IOSTD, value DTAPI_IOCONFIG_HDSDI
#define DTAPI_IOCONFIG_1080I50     40            // 1080i/50
#define DTAPI_IOCONFIG_1080I59_94  41            // 1080i/59.94
#define DTAPI_IOCONFIG_1080I60     42            // 1080i/60
#define DTAPI_IOCONFIG_1080P23_98  43            // 1080p/23.98
#define DTAPI_IOCONFIG_1080P24     44            // 1080p/24
#define DTAPI_IOCONFIG_1080P25     45            // 1080p/25
#define DTAPI_IOCONFIG_1080P29_97  46            // 1080p/29.97
#define DTAPI_IOCONFIG_1080P30     47            // 1080p/30
#define DTAPI_IOCONFIG_720P23_98   48            // 720p/23.98
#define DTAPI_IOCONFIG_720P24      49            // 720p/24
#define DTAPI_IOCONFIG_720P25      50            // 720p/25
#define DTAPI_IOCONFIG_720P29_97   51            // 720p/29.97
#define DTAPI_IOCONFIG_720P30      52            // 720p/30
#define DTAPI_IOCONFIG_720P50      53            // 720p/50
#define DTAPI_IOCONFIG_720P59_94   54            // 720p/59.94
#define DTAPI_IOCONFIG_720P60      55            // 720p/60

// SubValues for group DTAPI_IOCONFIG_IOSTD, value DTAPI_IOCONFIG_SDI
#define DTAPI_IOCONFIG_525I59_94   56            // 525i/59.94
#define DTAPI_IOCONFIG_625I50      57            // 625i/50

// SubValues for group DTAPI_IOCONFIG_IOSTD, value DTAPI_IOCONFIG_SPISDI
#define DTAPI_IOCONFIG_SPI525I59_94 58           // SPI 525i/59.94
#define DTAPI_IOCONFIG_SPI625I50   59            // SPI 625i/50

// Values for group IO_CONFIG_RFCLKSEL (RF clock source selection)
#define DTAPI_IOCONFIG_RFCLKEXT    60            // External RF clock input
#define DTAPI_IOCONFIG_RFCLKINT    61            // Internal RF clock reference

// Values for group IO_CONFIG_SPICLKSEL (Parallel port clock source selection)
#define DTAPI_IOCONFIG_SPICLKEXT   62            // External clock input
#define DTAPI_IOCONFIG_SPICLKINT   63            // Internal clock reference

// Values for group IO_CONFIG_SPIMODE (Parallel port mode)
#define DTAPI_IOCONFIG_SPIFIXEDCLK 64            // SPI fixed clock with valid signal
#define DTAPI_IOCONFIG_SPIDVBMODE  65            // SPI DVB mode
#define DTAPI_IOCONFIG_SPISER8B    66            // SPI serial 8-bit mode
#define DTAPI_IOCONFIG_SPISER10B   67            // SPI serial 10-bit mode

// Values for group IO_CONFIG_SPISTD (Parallel port I/O standard)
#define DTAPI_IOCONFIG_SPILVDS1    68            // LVDS1
#define DTAPI_IOCONFIG_SPILVDS2    69            // LVDS2
#define DTAPI_IOCONFIG_SPILVTTL    70            // LVTTL

// Values for group IO_CONFIG_TSRATESEL (Transport-stream rate selection)
#define DTAPI_IOCONFIG_EXTTSRATE   71            // External TS rate clock input
#define DTAPI_IOCONFIG_EXTRATIO    72            // External TS rate clock with ratio
#define DTAPI_IOCONFIG_INTTSRATE   73            // Internal TS rate clock reference
#define DTAPI_IOCONFIG_LOCK2INP    74            // Lock TS rate to input port

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIqData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure representing an I/Q data sample
//
struct DtIqData
{
    int  m_I, m_Q;                  // I/Q sample pair
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbsLayerPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// DtIsdbsLayerPars
//
struct DtIsdbsLayerPars
{
    int  m_NumSlots;                // Number of slots
    int  m_ModCod;                  // Modulation method and Code rate
};

// Number of slots per ISDB-S frame
#define DTAPI_ISDBS_SLOTS_PER_FRAME 48

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbsPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// ISDB-S parameters including per-layer parameters
//
struct DtIsdbsPars
{
    DtIsdbsPars() : m_DoMux(false) {}

    bool  m_DoMux;                  // Hierarchical multiplexing yes/no
    int   m_Emergency;              // Switch-on control for emergency broadcast
    int   m_RelTs2TsId[8];          // Relative TS to TS-ID mapping
    // Slot to relative TS mapping
    int   m_Slot2RelTs[DTAPI_ISDBS_SLOTS_PER_FRAME];
    // Modulation parameters per hierarchical layer
    DtIsdbsLayerPars  m_LayerPars[4];

    DTAPI_RESULT  CheckValidity(void);
    void  Init(void);
    bool  operator == (DtIsdbsPars& Rhs);
    bool  operator != (DtIsdbsPars& Rhs);
};

// ISDB-S modulation method and code rate
#define DTAPI_ISDBS_MODCOD_BPSK_1_2 1
#define DTAPI_ISDBS_MODCOD_QPSK_1_2 2
#define DTAPI_ISDBS_MODCOD_QPSK_2_3 3
#define DTAPI_ISDBS_MODCOD_QPSK_3_4 4
#define DTAPI_ISDBS_MODCOD_QPSK_5_6 5
#define DTAPI_ISDBS_MODCOD_QPSK_7_8 6
#define DTAPI_ISDBS_MODCOD_8PSK_2_3 7
#define DTAPI_ISDBS_MODCOD_NOT_ALLOC 15

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbtLayerData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Parameters per hierarchical ISDB-T layer used for statistic: DTAPI_STAT_ISDBT_PARSDATA
//
struct DtIsdbtLayerData
{
    int  m_NumSegments;             // Number of segments
    int  m_Modulation;              // Modulation type
    int  m_CodeRate;                // Code rate
    int  m_TimeInterleave;          // Time interleaving 0..4 (new spec limits the
                                    // maximum value to 3 instead of 4).
                                    // Time interleaving I = 0 if ti = 0 or
                                    // I = (1 << (ti + 2 - mode)) if ti != 0
    DtIsdbtLayerData();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbtLayerPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Parameters per hierarchical ISDB-T layer
//
struct DtIsdbtLayerPars
{
    int  m_NumSegments;             // Number of segments
    int  m_Modulation;              // Modulation type
    int  m_CodeRate;                // Code rate
    int  m_TimeInterleave;          // Time interleaving
    // Derived:
    int  m_BitRate;                 // Bits per second assuming 6 MHz channel
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbtParamsData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// ISDB-T parameters including per-layer parameters used for statistic: 
// DTAPI_STAT_ISDBT_PARSDATA
//
struct DtIsdbtParamsData
{
    int  m_BType;                   // Broadcast type
    int  m_Mode;                    // Transmission mode: 1, 2 or 3 
    int  m_Guard;                   // Guard interval
    int  m_PartialRx;               // Use first layer for partial reception.
                                    // Ignored for radio broadcasts 
    // Layer-A/B/C parameters
    DtIsdbtLayerData  m_LayerPars[3];

    DtIsdbtParamsData();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbtPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// ISDB-T parameters including per-layer parameters
//
struct DtIsdbtPars
{
    bool  m_DoMux;                  // Hierarchical multiplexing yes/no
    bool  m_FilledOut;              // Members have been given a value
    int  m_ParXtra0;                // #Segments, bandwidth, sample rate, sub channel
    int  m_BType;                   // Broadcast type
    int  m_Mode;                    // Transmission mode
    int  m_Guard;                   // Guard interval
    int  m_PartialRx;               // Partal reception
    int  m_Emergency;               // Switch-on control for emergency broadcast
    int  m_IipPid;                  // PID used for multiplexing IIP packet
    int  m_LayerOther;              // Other PIDs are mapped to this layer
    int  m_Virtual13Segm;           // Virtual 13-segment mode
    
    // Layer-A/B/C parameters
    DtIsdbtLayerPars  m_LayerPars[3];

    // PID-to-layer mapping
    std::map<int, int>  m_Pid2Layer;

    // Derived:
    bool  m_Valid;                  // The parameter set is valid
    int  m_TotalBitrate;            // Bitrate of entire stream

    // Member function
    DtIsdbtPars();
    static bool  BTypeCompat(int BType, int NumSegm);
    DTAPI_RESULT  CheckValidity(int& ResultCode);
    DTAPI_RESULT  ComputeRates(void);
    void  MakeConsistent();
    void  MakeNumSegmConsistent();
    int  NumSegm();
    DTAPI_RESULT  RetrieveParsFromTs(char* pBuffer, int NumBytes);
    bool  operator == (DtIsdbtPars& Rhs);
    bool  operator != (DtIsdbtPars& Rhs);
};

 


// ISDB-T layer selection used for demodulation
#define DTAPI_ISDBT_LAYER_NONE     -1
#define DTAPI_ISDBT_LAYER_AUTO     -2

// PID-to-layer mapping
#define DTAPI_ISDBT_LAYER_A         1
#define DTAPI_ISDBT_LAYER_B         2
#define DTAPI_ISDBT_LAYER_C         4

// DtIsdbtPars.m_BType - Broadcast type
#define DTAPI_ISDBT_BTYPE_TV        0            // 1/3/13-segment TV broadcast
#define DTAPI_ISDBT_BTYPE_RAD1      1            // 1-segment radio broadcast
#define DTAPI_ISDBT_BTYPE_RAD3      2            // 3-segment radio broadcast

// DtIsdbtPars.m_Guard - Guard interval
#define DTAPI_ISDBT_GUARD_1_32      0
#define DTAPI_ISDBT_GUARD_1_16      1
#define DTAPI_ISDBT_GUARD_1_8       2
#define DTAPI_ISDBT_GUARD_1_4       3

// DtIsdbtLayerPars.m_Modulation - Modulation type
#define DTAPI_ISDBT_MOD_DQPSK       0
#define DTAPI_ISDBT_MOD_QPSK        1
#define DTAPI_ISDBT_MOD_QAM16       2
#define DTAPI_ISDBT_MOD_QAM64       3

// DtIsdbtLayerPars.m_CodeRate - Code rate
#define DTAPI_ISDBT_RATE_1_2        0
#define DTAPI_ISDBT_RATE_2_3        1
#define DTAPI_ISDBT_RATE_3_4        2
#define DTAPI_ISDBT_RATE_5_6        3
#define DTAPI_ISDBT_RATE_7_8        4

// DtOutpChannel::SetModControl - Initial Total Number of Segments
#define DTAPI_ISDBT_SEGM_1          0x00000001
#define DTAPI_ISDBT_SEGM_3          0x00000003
#define DTAPI_ISDBT_SEGM_13         0x0000000D
#define DTAPI_ISDBT_SEGM_MSK        0x0000000F

// DtOutpChannel::SetModControl - Bandwidth
#define DTAPI_ISDBT_BW_5MHZ         0x00000010
#define DTAPI_ISDBT_BW_6MHZ         0x00000020
#define DTAPI_ISDBT_BW_7MHZ         0x00000030
#define DTAPI_ISDBT_BW_8MHZ         0x00000040
#define DTAPI_ISDBT_BW_MSK          0x000000F0

// DtOutpChannel::SetModControl - Sample Rate
#define DTAPI_ISDBT_SRATE_1_1       0x00000100
#define DTAPI_ISDBT_SRATE_1_2       0x00000200
#define DTAPI_ISDBT_SRATE_1_4       0x00000300
#define DTAPI_ISDBT_SRATE_1_8       0x00000400
#define DTAPI_ISDBT_SRATE_27_32     0x00000500
#define DTAPI_ISDBT_SRATE_135_64    0x00000600
#define DTAPI_ISDBT_SRATE_MSK       0x00000F00

// DtOutpChannel::SetModControl - Sub Channel
#define DTAPI_ISDBT_SUBCH_MSK       0x0003F000
#define DTAPI_ISDBT_SUBCH_SHIFT     12

// Result codes for DtIsdbtPars::CheckValidity
#define DTAPI_ISDBT_OK              0
#define DTAPI_ISDBT_E_BTYPE         1
#define DTAPI_ISDBT_E_NSEGM         2
#define DTAPI_ISDBT_E_PARTIAL       3
#define DTAPI_ISDBT_E_NOT_FILLED    4
#define DTAPI_ISDBT_E_SUBCHANNEL    5
#define DTAPI_ISDBT_E_SRATE         6
#define DTAPI_ISDBT_E_BANDWIDTH     7
#define DTAPI_ISDBT_E_MODE          8
#define DTAPI_ISDBT_E_GUARD         9


//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtMatrixInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
struct  DtMatrixInfo
{
    int  m_VidStd;                  // Video standard
    int  m_Scaling;                 // Scaled frame format
    int  m_NumColumns;              // Number of columns 
};

// Maximum number of fading paths, used in DtModPars
#define DTAPI_MAX_OUTPUTS           16

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtModPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for storing a complete set of modulation parameters.
//
// NOTE FOR ISDB-T: DtModPars.m_ParXtra0 is never used, all ISDB-T parameters are stored
// in DtIsdbtPars (in *m_pXtraPars).
//
struct DtModPars
{
    // Modulation parameters
    int  m_ModType;                 // Modulation type; -1 = not set
    int  m_ParXtra0;                // ParXtra0 (Code Rate / J.83 Annex); -1 = not set
    int  m_ParXtra1;                // ParXtra1; -1 = not set
    int  m_ParXtra2;                // ParXtra2; -1 = not set
    void*  m_pXtraPars;             // Extra CMMB/ISDB-S/ISDB-T/DVB-C2/DVB-T2 parameters

    // Rates; Set to -1 (not set) by SetModControl, except for symbol rate which is set
    // to a default value for modulation types that support a symbol rate.
    // If both symbol and TS rate are set, TS rate takes precedence.
    int  m_SymRate;                 // Symbol rate in baud
    int  m_TsRate;                  // Transport-stream rate in bps

    // Channel modelling per output channel
    bool  m_IsCmEnable[DTAPI_MAX_OUTPUTS];
                                    // Channel modelling is enabled yes/no
    DtCmPars  m_CmPars[DTAPI_MAX_OUTPUTS];
                                    // Channel modelling parameters

    // Custom roll-off filter
    bool  m_IsRoEnable;             // Custom roll-off filter enable yes/no
    DtFilterPars  m_RollOffFilter;  // Custom roll-off filter parameters

    // Miscellaneous
    int  m_OutputLevel;             // Output level; -9999 = not set
    __int64  m_RfFreqHz;            // RF frequency in Hz
    unsigned char  m_S2PlHdrScrSeq[12];
                                    // DVB-S2 PL header scrambling sequence
    int  m_TxMode;                  // Transmit mode; Included here because packet
                                    // size affects the modulation process
    int  m_StuffMode;               // Stuffing mode

    // Operations
    DTAPI_RESULT  CheckPars();
    DTAPI_RESULT  SetModControl(int ModType, int, int, int, void* pXtraPars);

    // Member functions
    DtCmmbPars*  pCmmbPars()    { return (DtCmmbPars*)m_pXtraPars; }
    DtDvbC2Pars*  pDvbC2Pars()  { return (DtDvbC2Pars*)m_pXtraPars; }
    DtDvbS2Pars*  pDvbS2Pars()  { return (DtDvbS2Pars*)m_pXtraPars; }
    DtDvbT2Pars*  pDvbT2Pars()  { return (DtDvbT2Pars*)m_pXtraPars; }
    DtIsdbsPars*  pIsdbsPars()  { return (DtIsdbsPars*)m_pXtraPars; }
    DtIsdbtPars*  pIsdbtPars()  { return (DtIsdbtPars*)m_pXtraPars; }
    DtIsdbTmmPars*  pIsdbTmmPars()  { return (DtIsdbTmmPars*)m_pXtraPars; }

    // Predicates
    bool  HasSymRate();
    bool  IsAdtbT(), IsAdtbtDtmb(), IsAtsc(), IsAtscMh(), IsCmmb(), IsCmEnable(int i=0);
    bool  IsDab();
    bool  IsDtmb(), IsDvbC2(), IsDvbS(), IsDvbS2(), IsDvbS2Apsk(), IsDvbS2L3(),
          IsDvbS2X(), IsDvbS2XL3();
    bool  IsDvbS2Mux();
    bool  IsDvbT(), IsDvbT2(), IsIqDirect(), IsIsdbS(), IsIsdbT(), IsIsdbTmm();
    bool  IsModTypeSet(), IsOfdm(), IsQam(), IsQamA(), IsQamB(), IsQamC(), IsQamAC();
    bool  IsRoEnable(), IsT2Mi();
    bool  RequiresMplpMod();

    // Constructor, destructor
    DtModPars();
    ~DtModPars();
private:
    // No implementation is provided for the copy constructor
    DtModPars(const DtModPars&);

private:
    void  CleanUpXtraPars();
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtPar .-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Generic structure to represent a single parameter setting.
// It is used in the advanced demodulator for setting and retrieving parameter settings.
//
struct DtPar
{
    DtPar();
    DtPar(int ParId);               // Constructor with DTAPI_PAR_xxx initialization
    virtual ~DtPar();

    // Value types supported for parameters
    enum ParValueType
    {
        PAR_VT_UNDEFINED, PAR_VT_BOOL, PAR_VT_DOUBLE, PAR_VT_INT
    };

    DTAPI_RESULT  m_Result;         // Result of retrieving the parameters
    int  m_ParId;                   // Identifies the parameter: DTAPI_PAR_XXX
    int  m_IdXtra[4];               // Extra identification parameters
    ParValueType  m_ValueType;      // Value type of parameter: PAR_VT_XXX
    union {
        bool  m_ValueBool;          // Value if value type is PAR_VT_BOOL
        double  m_ValueDouble;      // Value if value type is PAR_VT_DOUBLE
        int  m_ValueInt;            // Value if value type is PAR_VT_INT
        void* m_pValue;             // Pointer for complex types
    };
    void  Cleanup();
    DTAPI_RESULT  GetName(const char*& pName, const char*& pShortName);
    DTAPI_RESULT  GetName(const wchar_t*& pName, const wchar_t*& pShortName);
    DTAPI_RESULT  GetValue(int &Value);
    DTAPI_RESULT  GetValue(double &Value);
    DTAPI_RESULT  GetValue(bool &Value);
    DTAPI_RESULT  SetId(int ParameterId);
    DTAPI_RESULT  SetValue(int Value);
    DTAPI_RESULT  SetValue(double Value);
    DTAPI_RESULT  SetValue(bool Value);

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);

    // Serialisation of an array of parameters
    static DTAPI_RESULT  FromXml(const std::wstring&, DtPar*& pPars, int& Count);
    static DTAPI_RESULT  ToXml(DtPar* pPars, int Count, std::wstring& XmlString);

    // Assignment operator
    DtPar&  operator=(const DtPar&);

private:
    // No implementation is provided for the copy constructor
    DtPar(const DtPar&);
};

// Integer parameters
#define DTAPI_PAR_DEMOD_THREADS     0x001        // Number of Threads/CPU cores used for
                                                 // software demodulation: default 4
#define DTAPI_PAR_DEMOD_LDPC_MAX    0x002        // LDPC maximum iterations: default 50
#define DTAPI_PAR_DEMOD_LDPC_AVG    0x003        // LDPC average iterations limit, used
                                                 // to limit CPU load; default 16
// Boolean parameters
#define DTAPI_PAR_DEMOD_MER_ENA     0x004        // Enable MER calculation; default on
// Undefined parameter
#define DTAPI_PAR_UNDEFINED         0x000        // Value is not defined yet

// Unsported item values
#define DTAPI_PAR_UNSUP_INTITEM     0x80000000   // Unsupported integer item
#define DTAPI_PAR_UNSUP_UINTITEM    0xFFFFFFFF   // Unsupported unsigned integer item

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtRawIpHeader -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Header placed in front of all IP Packets when DTAPI_RXMODE_IPRAW mode is used
//
struct DtRawIpHeader
{
    unsigned short  m_Tag;          // 0x44A0h = \91D\92160
    unsigned short  m_Length;       // IP packet length
    unsigned int  m_TimeStamp;      // IP packet arrival/transmit timestamp
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtStatistic -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
struct DtStatistic
{
    DtStatistic();
    DtStatistic(int StatisticId);   // Constructor with DTAPI_STAT_xxx initialization
    virtual ~DtStatistic();

    // Value types supported for statistics
    // NOTE: ALWAYS ADD NEW TYPES TO END OF LIST, FOR BACKWARDS COMPATIBILITY
    enum StatValueType
    {
        STAT_VT_UNDEFINED, STAT_VT_BOOL, STAT_VT_DOUBLE, STAT_VT_INT, 
        STAT_VT_DVBC2_L1P2, STAT_VT_DVBC2_PLPSIG, STAT_VT_DVBT2_L1,
        STAT_VT_ISDBT_PARS, STAT_VT_LDPC_STATS, STAT_VT_MA_DATA,
        STAT_VT_MA_STATS, STAT_VT_PLP_BLOCKS, STAT_VT_VIT_STATS,
        STAT_VT_DAB_ENSEM, STAT_VT_RS_STATS, STAT_VT_DVBT_TPS
    };


    DTAPI_RESULT  m_Result;         // Result of retrieving the statistic
    int  m_StatisticId;             // Identifies the statistic: DTAPI_STAT_XXX
    int  m_IdXtra[4];               // Extra identification parameters
    StatValueType  m_ValueType;     // Value type of statistic: STAT_VT_XXX
    union {
        bool  m_ValueBool;          // Value if value type is STAT_VT_BOOL
        double  m_ValueDouble;      // Value if value type is STAT_VT_DOUBLE
        int  m_ValueInt;            // Value if value type is STAT_VT_INT
        void*  m_pValue;            // Pointer if value type is STAT_VT_DVBC2_L1P2,
                                    // STAT_VT_DVBC2_PLPSIG, STAT_VT_DVBT_TPS,
                                    // STAT_VT_DVBT2_L1, STAT_VT_VIT_STATS
                                    // STAT_VT_DAB_ENSEM or STAT_VT_RS_STATS
    };
    void  Cleanup();
    DTAPI_RESULT  GetName(const char*& pName, const char*& pShortName);
    DTAPI_RESULT  GetName(const wchar_t*& pName, const wchar_t*& pShortName);
    DTAPI_RESULT  GetValue(int &Value);
    DTAPI_RESULT  GetValue(double &Value);
    DTAPI_RESULT  GetValue(bool &Value);
    DTAPI_RESULT  GetValue(DtDabEnsembleInfo*& pValue);
    DTAPI_RESULT  GetValue(DtDvbC2DemodL1Part2Data*& pValue);
    DTAPI_RESULT  GetValue(DtDvbC2DemodL1PlpSigData*& pValue);
    DTAPI_RESULT  GetValue(DtDvbTTpsInfo*& pValue);
    DTAPI_RESULT  GetValue(DtDvbT2DemodL1Data*& pValue);
    DTAPI_RESULT  GetValue(DtDemodLdpcStats*& pValue);
    DTAPI_RESULT  GetValue(DtDemodMaLayerData*& pValue);
    DTAPI_RESULT  GetValue(DtDemodMaLayerStats*& pValue);
    DTAPI_RESULT  GetValue(DtDemodPlpBlocks*& pValue);
    DTAPI_RESULT  GetValue(DtIsdbtParamsData*& pValue);
    DTAPI_RESULT  GetValue(DtRsDecStats*& pValue);
    DTAPI_RESULT  GetValue(DtVitDecStats*& pValue);
    DTAPI_RESULT  SetId(int StatisticId);
    DTAPI_RESULT  SetValue(int Value);
    DTAPI_RESULT  SetValue(double Value);
    DTAPI_RESULT  SetValue(bool Value);
    DTAPI_RESULT  SetValue(DtDabEnsembleInfo& pValue);
    DTAPI_RESULT  SetValue(DtDvbC2DemodL1Part2Data& Value);
    DTAPI_RESULT  SetValue(DtDvbC2DemodL1PlpSigData& Value);
    DTAPI_RESULT  SetValue(DtDvbTTpsInfo& pValue);
    DTAPI_RESULT  SetValue(DtDvbT2DemodL1Data& Value);
    DTAPI_RESULT  SetValue(DtDemodLdpcStats& Value);
    DTAPI_RESULT  SetValue(DtDemodMaLayerData& Value);
    DTAPI_RESULT  SetValue(DtDemodMaLayerStats& Value);
    DTAPI_RESULT  SetValue(DtDemodPlpBlocks& Value);
    DTAPI_RESULT  SetValue(DtIsdbtParamsData& Value);
    DTAPI_RESULT  SetValue(DtRsDecStats& pValue);
    DTAPI_RESULT  SetValue(DtVitDecStats& pValue);

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);

    // Serialisation of an array of statistics
    static DTAPI_RESULT  FromXml(const std::wstring&, DtStatistic*&, int& Count);
    static DTAPI_RESULT  ToXml(DtStatistic* pStatistics, int Count, std::wstring&);

    // Assignment operator
    DtStatistic&  operator=(const DtStatistic&);

private:
    // No implementation is given of the copy constructor
    DtStatistic(const DtStatistic&);
};

// Integer statistics
#define DTAPI_STAT_BADPCKCNT        0x003        // Count of uncorrected packets
#define DTAPI_STAT_CNR              0x105        // Carrier-to-noise ratio in dB
#define DTAPI_STAT_DVBC2_DSLICEDISC 0x010        // DVB-C2 Data slice discontinuity count
#define DTAPI_STAT_DVBC2_L1HDR_ERR  0x00E        // DVB-C2 L1 Preamble header error count
#define DTAPI_STAT_DVBC2_L1P2_ERR   0x00F        // DVB-C2 L1-Part 2 error count
#define DTAPI_STAT_DVBT2_L1PRE_ERR  0x00C        // DVB-T2 L1-Pre error count
#define DTAPI_STAT_DVBT2_L1POST_ERR 0x00D        // DVB-T2 L1-Post error count
#define DTAPI_STAT_EBN0             0x111        // Eb/N0 in dB (estimated on MER)
#define DTAPI_STAT_ESN0             0x110        // Es/N0 in dB (estimated on MER)
#define DTAPI_STAT_LINKMARGIN       0x10F        // Link margin in dB
#define DTAPI_STAT_MER              0x106        // Modulation error rate in dB
#define DTAPI_STAT_MOD_SAT          0x004        // Modulator saturation count
#define DTAPI_STAT_RELOCKCNT        0x00A        // Receiver re-lock count
#define DTAPI_STAT_RFLVL_CHAN       0x005        // RF power level for channel bandwidth
#define DTAPI_STAT_RFLVL_CHAN_QS    0x015        // Quick scan of channel level
#define DTAPI_STAT_RFLVL_NARROW     0x006        // RF power level for a narrow bandwidth
#define DTAPI_STAT_RFLVL_NARROW_QS  0x016        // Quick scan of channel level
#define DTAPI_STAT_RS               0x008        // Reed-Solomon error counter
#define DTAPI_STAT_SNR              0x107        // Signal-to-noise ratio in dB
#define DTAPI_STAT_TEMP_TUNER       0x009        // Tuner temperature
#define DTAPI_STAT_T2MI_OVFS        0x00B        // DVB-T2 T2-MI overflow count

// Double statistics
#define DTAPI_STAT_BER_POSTBCH      0x100        // Post-BCH bit error rate
#define DTAPI_STAT_BER_POSTLDPC     0x101        // Post-LDPC bit error rate
#define DTAPI_STAT_BER_POSTVIT      0x102        // Post-Viterbi bit error rate
#define DTAPI_STAT_BER_PREBCH       0x10D        // Pre-BCH bit error rate
#define DTAPI_STAT_BER_PRELDPC      0x10E        // Pre-LDPC bit error rate
#define DTAPI_STAT_BER_PRERS        0x103        // Pre-Reed-Solomon bit error rate
#define DTAPI_STAT_BER_PREVIT       0x104        // Pre-Viterbi bit error rate
#define DTAPI_STAT_FER_POSTBCH      0x116        // Post-BCH frame error rate
#define DTAPI_STAT_FREQ_SHIFT       0x10B        // Input frequency shift (Hz)
#define DTAPI_STAT_OCCUPIEDBW       0x112        // Occupied bandwidth
#define DTAPI_STAT_PER              0x108        // Packet error rate
#define DTAPI_STAT_ROLLOFF          0x113        // Roll-off factor in percentage
#define DTAPI_STAT_SAMPRATE_OFFSET  0x10C        // Sample rate offset (ppm) 

// Boolean lock statistics
#define DTAPI_STAT_CARRIER_LOCK     0x201        // Carrier lock
#define DTAPI_STAT_FEC_LOCK         0x202        // FEC lock
#define DTAPI_STAT_LOCK             0x200        // Overall lock status
#define DTAPI_STAT_PACKET_LOCK      0x203        // Packet lock
#define DTAPI_STAT_SPECTRUMINV      0x205        // Spectrum inversion
#define DTAPI_STAT_VIT_LOCK         0x204        // Viterbi lock

// Complex statistics
#define DTAPI_STAT_DAB_ENSEM_INFO   0x308        // DAB ensemble information from the
                                                 // Fast Information Channel (FIC)
#define DTAPI_STAT_DVBC2_L1P2DATA   0x300        // DVB-C2 Layer-1 Part 2 data
#define DTAPI_STAT_DVBC2_PLPSIGDATA 0x301        // DVB-C2 Layer-1 PLP signalling data
#define DTAPI_STAT_DVBT_TPS_INFO    0x30B        // DVB-T TPS information
#define DTAPI_STAT_DVBT2_L1DATA     0x302        // DVB-T2 Layer-1 data
#define DTAPI_STAT_ISDBT_PARSDATA   0x303        // ISDB-T parameters data
#define DTAPI_STAT_LDPC_STATS       0x304        // DVB-C2/T2 LDPC statistics
#define DTAPI_STAT_MA_DATA          0x305        // DVB-C2/T2 mode adaptation data
#define DTAPI_STAT_MA_STATS         0x306        // DVB-C2/T2 mode adaptation statistics
#define DTAPI_STAT_PLP_BLOCKS       0x307        // DVB-C2/T2 PLP number of FEC blocks
#define DTAPI_STAT_RSDEC_STATS      0x30A        // Reed-Solomon decoder statistics
#define DTAPI_STAT_VITDEC_STATS     0x309        // Viterbi decoder statistics 

// DekTec internal statistics
#define DTAPI_STAT_AGC1             0x001        // First AGC value
#define DTAPI_STAT_AGC2             0x002        // Second AGC value
#define DTAPI_STAT_RFLVL_UNCALIB    0x007        // Uncalibrated RF power level
#define DTAPI_STAT_RFLVL_UNCALIB_DBM 0x10A       // Uncalibrated RF power level in dBm
#define DTAPI_STAT_SYNTAX_ERR_CNT   0x114        // Num syntax errors in usb bitstream
#define DTAPI_STAT_OVERFLOW_CNT     0x115        // Number of dtapi<>kernel buf overflows

// Unsupported item values
#define DTAPI_STAT_UNDEFINED        0x000        // Value is not defined yet
#define DTAPI_STAT_UNSUP_INTITEM    0x80000000   // Unsupported integer item
#define DTAPI_STAT_UNSUP_UINTITEM   0xFFFFFFFF   // Unsupported unsigned integer item

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtTransmitter -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure describing a transmitter. Used by DtInpChannel::BlindScan to return the
// transmitters found by scanning a frequency band.
//
struct DtTransmitter
{
    __int64  m_FreqHz;              // Center frequency of the transmitter
    int  m_ModType;                 // Modulation type
    int  m_SymbolRate;              // Symbol rate
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for storing TSoIP parameters
//
struct DtIpPars
{
public:
    unsigned char  m_Ip[16];        // IP address (IPv4/IPv6)
    unsigned short  m_Port;         // Port number
    unsigned char  m_SrcFltIp[16];  // Source filter: IP address (IPv4/IPv6)
    unsigned short  m_SrcFltPort;   // Source filter: port number
    int  m_VlanId;                  // VLAN ID
    int  m_VlanPriority;            // VLAN priority

    // Redundant link
    unsigned char  m_Ip2[16];       // IP address (IPv4/IPv6)
    unsigned short  m_Port2;        // Port number
    unsigned char  m_SrcFltIp2[16]; // Source filter: IP address (IPv4/IPv6)
    unsigned short  m_SrcFltPort2;  // Source filter: port number
    int  m_VlanId2;                 // VLAN ID
    int  m_VlanPriority2;           // VLAN priority

    int  m_TimeToLive;              // Time-to-Live setting for IP TX
    int  m_NumTpPerIp;              // #TPs per IP packet
    int  m_Protocol;                // Protocol: DTAPI_PROTO_UDP/RTP
    int  m_DiffServ;                // Differentiated services
    int  m_FecMode;                 // Error correction mode: DTAPI_FEC_DISABLE/2D
    int  m_FecNumRows;              // \91D\92 = #rows in FEC matrix
    int  m_FecNumCols;              // \91L\92 = #columns in FEC matrix
    int  m_Flags;                   // Optional control/status flags: IPv4/IPv6
    int  m_Mode;                    // DTAPI_IP_NORMAL, DTAPI_IP_TX_DBLBUF,
                                    // DTAPI_IP_RX_DBLBUF, DTAPI_IP_RX_FAILOVER
public:
    DtIpPars();
    ~DtIpPars();
};

// Legacy
#define DtTsIpPars  DtIpPars

// Error correction modes (DtIpPars::m_FecMode)
#define DTAPI_FEC_DISABLE           0
#define DTAPI_FEC_2D                1            // FEC reconstruction
#define DTAPI_FEC_2D_M1             1            // Mode1: FECdT = DVBdT + .5 * DVBdT
#define DTAPI_FEC_2D_M2             2            // Mode2: FECdT = DVBdT

// Optional control/status flags (DtIpPars::m_Flags)
#define DTAPI_IP_V4                 0x00
#define DTAPI_IP_V6                 0x01
#define DTAPI_IP_TX_MANSRCPORT      0x10

// Transmission/reception mode (DtIpPars::m_Mode)
#define DTAPI_IP_NORMAL             0
#define DTAPI_IP_TX_DBLBUF          1
#define DTAPI_IP_RX_DBLBUF          2
#define DTAPI_IP_RX_FAILOVER        3

// IP protocol (DtIpPars::m_Protocol)
#define DTAPI_PROTO_UDP             0
#define DTAPI_PROTO_RTP             1
#define DTAPI_PROTO_AUTO            2
#define DTAPI_PROTO_UNKN            2

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIpStat -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for retrieving IP statistics from drivers.
//
struct DtIpStat
{
    unsigned int  m_TotNumIpPackets;
    unsigned int  m_LostIpPacketsBeforeFec;      // #Lost IP packets before FEC 
    unsigned int  m_LostIpPacketsAfterFec;       // #Lost IP packets after FEC
};

// Legacy
#define DtTsIpStat DtIpStat

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtTunePars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Structure for setting tuner specific parameters
//
struct DtTunePars
{
    union {
        // DTA-2131 specific tuner parameters
        struct {
            int  m_TunerStandard;   // DTAPI_TUNMOD_xxx
            int  m_TunerBandwidth;  // Tuning bandwidth in Hz
            int  m_IfFrequency;     // IF frequency in Hz
                                    // (-1 according tuner standard)
            int  m_LpfCutOff;       // Low-pass filter cutoff; DTAPI_TUN31_LPF_x
            int  m_LpfOffset;       // Low-pass filter offset; DTAPI_TUN31_LPF_x
            int  m_HiPass;          // Hi Pass filter; DTAPI_TUN31_HPF_x
            int  m_DcNotchIfPpf;    // Enable DC notch IF PPF; DTAPI_TUN31_NOTCH_x
            int  m_IfNotch;         // Enable IF notch; DTAPI_TUN31_NOTCH_x
            int  m_IfNotchToRssi;   // Enable IF notch to RSSI; DTAPI_TUN31_NOTCH_x
        } m_Dta2131TunePars;
    } u;
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtVidStdInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
struct DtVidStdInfo
{
    int  m_VidStd;                  // Video Standard
    bool  m_IsHd;                   // true: is an HD format: false: is SD format

    bool  m_IsInterlaced;           // Is interlaced
    int  m_NumLines;                // Number of lines per frame
    double  m_Fps;                  // Frame rate
    bool  m_IsFractional;           // Fractional framerate
        
    int  m_FrameNumSym;             // Size of frame (in # symbols)
    int  m_LineNumSym;              // # of symbol per line
    int  m_LineNumSymHanc;          // # of HANC symbols per line
    int  m_LineNumSymVanc;          // # of VANC symbols per line
    int  m_LineNumSymEav;           // # of EAV symbols per line
    int  m_LineNumSymSav;           // # of SAV symbols per line

    // Field 1
    int  m_Field1StartLine;         // Line # of first line for field 1
    int  m_Field1EndLine;           // Line # of last line for field 1
    int  m_Field1VidStartLine;      // Line # of first line containing active video
    int  m_Field1VidEndLine;        // Line # of last line containing active video
    
    // Field 2
    int  m_Field2StartLine;         // Line # of first line for field 2
    int  m_Field2EndLine;           // Line # of last line for field 2
    int  m_Field2VidStartLine;      // Line # of first line containing active video
    int  m_Field2VidEndLine;        // Line # of last line containing active video
};

//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DEMODULATION PARAMETERS +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

//.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDemodDvbS2ModCodSettings -.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for storing the DVB-S2 
// 
struct DtDemodDvbS2ModCodSettings
{
    bool  m_Enable;                 // Demodulation of this MODCOD (yes/no)
    int  m_SnrThreshold;            // SNR threshold of this MODCOD for automute algorithm
    DtDemodDvbS2ModCodSettings() : m_Enable(false), m_SnrThreshold(0) {}
    DtDemodDvbS2ModCodSettings(bool Enable, int SnrThreshold) : 
                                         m_Enable(Enable), m_SnrThreshold(SnrThreshold) {}
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Structure for storing a complete set of demodulation parameters
//
class DtDemodPars
{
public:
    DtDemodPars();
    DtDemodPars(const DtDemodPars&);
    ~DtDemodPars();
public:
    DTAPI_RESULT CheckValidity();
    int  GetModType() const;
    DTAPI_RESULT  SetModType(int ModType);
    DtDemodParsAtsc*  Atsc() const;
    DtDemodParsDab*  Dab() const;
    DtDemodParsDvbC2*  DvbC2() const;
    DtDemodParsDvbS*  DvbS() const;
    DtDemodParsDvbS2*  DvbS2() const;
    DtDemodParsDvbS2Adv*  DvbS2Adv() const; 
    DtDemodParsDvbT*  DvbT() const;
    DtDemodParsDvbT2*  DvbT2() const;
    DtDemodParsIq*  Iq() const;
    DtDemodParsIq2131*  Iq2131() const; 
    DtDemodParsIsdbt*  Isdbt() const;
    DtDemodParsQam*  Qam() const;

    // Predicates
    bool IsAtsc() const, IsDab() const, IsDvbC2() const, IsDvbS() const, 
        IsDvbS2() const, IsDvbT() const,IsDvbT2() const, IsIq() const, IsIq2131() const,
        IsIsdbt() const,  IsQam() const;

    // Operators
    void  operator=(const DtDemodPars& Pars);

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);

    // Conversion helper
    DTAPI_RESULT  FromOldStyle(int ModType, int ParXtra0, int ParXtra1, int ParXtra2);
    DTAPI_RESULT  ToOldStyle(int& ModType, int& ParXtra0, int& ParXtra1, int& ParXtra2);
private:
    int  m_ModType;                 // Modulation type
    void*  m_pDemodPars;            // Demodulation parameters; Type depends on m_ModType
private:
    void  CleanUpDemodPars();
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsAtsc -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Demodulation parameters for modulation type DTAPI_MOD_ATSC
//
struct DtDemodParsAtsc
{
    int  m_Constellation;           // VSB constellation
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDab -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Demodulation parameters for modulation type DTAPI_MOD_DAB
//
struct DtDemodParsDab
{
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbC2 -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_DVBC2
//
struct DtDemodParsDvbC2
{
    int  m_Bandwidth;               // Bandwidth
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbS -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Demodulation parameters for modulation type DTAPI_MOD_DVBS
//
struct DtDemodParsDvbS
{
    int  m_CodeRate;                // DVB-S coderate
    int  m_SpecInv;                 // Spectral inversion (yes/no)
    int  m_SymRate;                 // Symbol rate in baud
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbS2 -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_DVBS2
//
struct DtDemodParsDvbS2
{
    int  m_CodeRate;                // Coderate
    int  m_Pilots;                  // Pilots (yes/no)
    int  m_SpecInv;                 // Spectral inversion (yes/no)
    int  m_FecFrame;                // Long or short FECFRAME
    int  m_SymRate;                 // Symbol rate in baud
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbS2Adv -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Advanced demodulation parameters for modulation type DTAPI_MOD_DVBS2
//
struct DtDemodParsDvbS2Adv : DtDemodParsDvbS2
{
    bool  m_AutoMuteModCods;        // MODCODS with an SNR threshold above the current SNR
                                    // will not be demodulated
    int  m_HysteresisMargin;        // Margin to add on top of the SNR threshold before 
                                    // re-enabling a certain modcod, in units of 0.1 dB
    std::map<DtDvbS2ModCod, DtDemodDvbS2ModCodSettings>  m_ModCods;   
                                    // List with supported modcods 
    DtDemodParsDvbS2Adv();
    DTAPI_RESULT  DeleteModCod(DtDvbS2ModCod ModCod);
    DTAPI_RESULT  InitSnrThreshold(int TypeNumber);
    DTAPI_RESULT  SetModCod(DtDvbS2ModCod ModCod, DtDemodDvbS2ModCodSettings &Settings);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbT -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_DVBT
//
struct DtDemodParsDvbT
{
    int  m_CodeRate;                // Coderate
    int  m_Bandwidth;               // Bandwidth
    int  m_Constellation;           // Constellation
    int  m_Guard;                   // Guard interval
    int  m_Interleaving;            // Interleaving
    int  m_Mode;                    // Transmission mode
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsDvbT2 -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_DVBT2
//
struct DtDemodParsDvbT2
{
    int  m_Bandwidth;               // Bandwidth
    int  m_T2Profile;               // DVB-T2 profile (Base/Lite)
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsIq -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_IQ
//
struct DtDemodParsIq
{
    int  m_Bandwidth;               // Signal bandwidth in Hz
    int  m_IqDemodType;             // IQ demodulation type (DTAPI_DEMOD_QAM or
                                    // DTAPI_DEMOD_OFDM)
    int  m_SampleRate;              // Sample rate in Hz
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsIq2131 -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_IQ_2131 (DTA-2131 specific)
//
struct DtDemodParsIq2131
{
    int  m_IqDemodFreq;             // IQ demodulation frequency in Hz
    DtFilterPars  m_LpfFilter;      // Anti-aliasing filter
    double  m_LpfScaleFactor;       // Scale factor after anti-aliasing filter
    int  m_SampleRate;              // Sample rate in Hz
    DtTunePars  m_TunePars;         // Tuning parameters
};

// IQ-demodulation type
#define  DTAPI_DEMOD_OFDM   0       // OFDM IQ-demodulation type
#define  DTAPI_DEMOD_QAM    1       // QAM IQ-demodulation type

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsIsdbt .-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_ISDBT
//
struct DtDemodParsIsdbt
{
    int  m_Bandwidth;               // Bandwidth DTAPI_ISDBT_BW_xMHZ
    int  m_SubChannel;              // Sub channel number, 0..41, default= 22
    int  m_NumberOfSegments;        // Number of segments DTAPI_ISDBT_SEGM_x
    DtDemodParsIsdbt() : m_SubChannel(22) {}
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodParsQam -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Demodulation parameters for modulation type DTAPI_MOD_QAMxxx
//
struct DtDemodParsQam
{
    int  m_Annex;                   // ITU-T J.83 Annex
    int  m_Interleaving;            // Interleaving; ignored for Annex A and C
    int  m_SymRate;                 // Symbol rate in baud  
};

//+=+=+=+=+=+=+=+=+=+=+=+=+=+ Common Demodulation Structures  +=+=+=+=+=+=+=+=+=+=+=+=+=+=

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodLdpcStats -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// LDCP statistic information for DVB-T2 and DVB-C2
//
struct DtDemodLdpcStats
{
    __int64  m_FecBlocksCount;      // #Decoded FEC blocks
    __int64  m_UncorrFecBlocksCount;// #Uncorrected FEC blocks after BCH (not exact)
    __int64  m_FecBlocksCount1;     // #Decoded FEC blocks, reset at the same time
                                    // as m_FecBlocksItCount/min/max
    __int64  m_FecBlocksItCount;    // Total #LDPC iterations
                                    // Average #LDPC iteration =
                                    //              m_FecBlocksItCount / m_FecBlocksCount1
    int  m_FecBlocksItMin;          // Minimum #LDPC iterations (-1 after reset)
    int  m_FecBlocksItMax;          // Maximum #LDPC iterations (-1 after reset)
    __int64  m_BchBitCount;         // #Decoded data bits, including BCH bits
    // Currently only data+BCH bits are taken into account (LDPC parity bits are ignored),
    // so the BER before LDPC is approximatively: m_BchBitErrorCount / m_BchBitCount
    // This is accurate only if there are no uncorrected blocks (m_UncorrFecBlocksCount=0)
    __int64  m_BchBitErrorCount;    // Bit error count before LDPC
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodMaLayerStats -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// LMode adaption layer statistics for DVB-T2 and DVB-C2
//
struct DtDemodMaLayerStats
{
    __int64  m_HdrCrc8ErrorCount;   // #CRC8 errors for BBframe header
    __int64  m_PckCrc8ErrorCount;   // #CRC8 errors for packets (only for m_Hem = 0)
    __int64  m_FramingErrorCount;   // SYNCD/DFL/UPL consistency errors
    __int64  m_CommonPlpResyncCount;// Number of times a resynchronization between data
                                    // and common PLP was needed. It normally happens
                                    // only in case of receive errors. This field is only
                                    // updated in the corresponding data PLP.
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtRsDecStats -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Reed-Solomon decoder info
//
struct DtRsDecStats
{
    bool    m_Locked;               // Decoder is locked     
    __int64  m_ByteSkipCount;       // Bytes skipped while looking for sync 
    __int64  m_PacketCount;         // Decoded packets 
    __int64  m_UncorrPacketCount;   // Uncorrected packets 
    __int64  m_ByteErrorCount;      // Byte error count  
    __int64  m_BitErrorCount;       // Bit error count
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtVitDecStats -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Viterbi decoder info (pre-viterbi BER) 
//
struct DtVitDecStats
{
    __int64  m_BitCount;            // Input bit count 
    __int64  m_BitErrorCount;       // Bit error count
};


//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodMaLayerData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Mode adaption layer info for DVB-T2 and DVB-C2
struct DtDemodMaLayerData
{
    bool  m_Hem;                    // High efficiency mode
    bool  m_Npd;                    // Null packet deletion
    int  m_Issy;                    // ISSY: mode, see DTAPI_DVBx2_ISSY_x
    int  m_IssyBufs;                // ISSY: current 'BUFS' value
    int  m_IssyTto;                 // ISSY: last 'TTO' value (DVB-T2 only)
    int  m_IssyBufStat;             // ISSY: last 'BUFSTAT' value (DVB-C2/S2 only)
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDemodPlpBlocks -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Number of FEC blocks per frame
//
struct DtDemodPlpBlocks
{
    int  m_NumBlocks;               // Last plp_num_blocks
    int  m_NumBlocksMin;            // Minimum plp_num_blocks (-1 = no new value since 
                                    // reset)
    int  m_NumBlocksMax;            // Maximum plp_num_blocks 
};

//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ MAIN DTAPI CLASSES +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDevice -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class representing a DekTec Device
//
class DtDevice
{
    // Constructor, destructor
public:
    DtDevice();
    virtual ~DtDevice();
private:
    // No implementation is provided for the copy constructor
    DtDevice(const DtDevice&);

    // Public access functions
public:
    virtual int  Category(void);
    virtual int  ChanType(int Port);
    virtual int  FirmwareVersion(void);
    virtual bool  IsAttached(void);
    virtual int  TypeNumber(void);
    virtual bool  HasCaps(int  Port, const DtCaps  Caps) const;

    // Public member functions
public:
    virtual DTAPI_RESULT  AttachToIpAddr(unsigned char Ip[4]);
    virtual DTAPI_RESULT  AttachToSerial(__int64 SerialNumber);
    virtual DTAPI_RESULT  AttachToSlot(int PciBusNumber, int SlotNumber);
    virtual DTAPI_RESULT  AttachToType(int TypeNumber, int DeviceNo=0);
    virtual DTAPI_RESULT  Detach(void);
    virtual DTAPI_RESULT  DetectIoStd(int  Port, int& Value, int& SubValue);
    virtual DTAPI_RESULT  FlashDisplay(int NumFlashes=5, int OnTime=100, int OffTime=100);
    virtual DTAPI_RESULT  GetAttribute(int AttrId, int& AttrValue);
    virtual DTAPI_RESULT  GetAttribute(int Port, int AttrId, int& AttrValue);
    virtual DTAPI_RESULT  GetAttribute(int Port, int AttrId, DtModPars& ModParVals, 
                                                                          int& AttrValue);
    virtual DTAPI_RESULT  GetDescriptor(DtDeviceDesc& DvcDesc);
    virtual DTAPI_RESULT  GetDeviceDriverVersion(int& Major, int& Minor, int& BugFix,
                                                                              int& Build);
    virtual DTAPI_RESULT  GetDisplayName(wchar_t* pName);
    virtual DTAPI_RESULT  GetDisplayName(char* pName);
    virtual DTAPI_RESULT  GetFanSpeed(int Fan, int& Rpm);
    virtual DTAPI_RESULT  GetFanTemperature(int Fan, int& Temp);
    virtual DTAPI_RESULT  GetFirmwareVersion(int& FirmwareVersion);
    virtual DTAPI_RESULT  GetGenlockState(int& State, int& RefVidStd);
    virtual DTAPI_RESULT  GetGenlockState(int& State);
    virtual DTAPI_RESULT  GetIoConfig(DtIoConfig& IoCfg);
    virtual DTAPI_RESULT  GetIoConfig(int Port, int Group, int& Value);
    virtual DTAPI_RESULT  GetIoConfig(int Port, int Group, int& Value, int& SubValue);
    virtual DTAPI_RESULT  GetIoConfig(int Port, int Group, int& Value, int& SubValue,
                                                                       __int64& ParXtra0);
    virtual DTAPI_RESULT  GetIoConfig(int Port, int Group, int& Value, int& SubValue,
                                                    __int64& ParXtra0, __int64& ParXtra1);
    virtual DTAPI_RESULT  GetNwSpeed(int Port, bool& Enable, int& Speed);
    virtual DTAPI_RESULT  GetRefClkCnt(int& RefClkCnt);
    virtual DTAPI_RESULT  GetRefClkCnt(int& RefClkCnt, int& RefClkFreqHz);
    virtual DTAPI_RESULT  GetRefClkFreq(int&  RefClkFreqHz);
    virtual DTAPI_RESULT  GetStateFlags(int  Port, int  &StateFlags);
    virtual DTAPI_RESULT  GetUsbSpeed(int& UsbSpeed);
    virtual DTAPI_RESULT  GetVcxoState(bool& Enable, int& Lock, int& VcxoClkFreqHz);
    virtual DTAPI_RESULT  HwFuncScan(int NumEntries, int& NumEntriesResult,
                                                                  DtHwFuncDesc* pHwFuncs);
    virtual DTAPI_RESULT  LedControl(int LedControl);
    virtual DTAPI_RESULT  RegisterCallback(pDtEventCallback Callback, void* pContext,
                                                       int EventTypes, void** pId = NULL);
    virtual DTAPI_RESULT  SetDisplayName(wchar_t* pName);
    virtual DTAPI_RESULT  SetDisplayName (char* pName);
    virtual DTAPI_RESULT  SetIoConfig(int Port, int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    virtual DTAPI_RESULT  SetIoConfig(DtIoConfig* pIoConfigs, int Count);
    virtual DTAPI_RESULT  SetNwSpeed(int Port, bool Enable, int Speed);
    virtual DTAPI_RESULT  UnregisterCallback(void* pId);
    virtual DTAPI_RESULT  VpdDelete(const char* pTag);
    virtual DTAPI_RESULT  VpdDelete(const wchar_t* pTag);
    virtual DTAPI_RESULT  VpdRead(const char* pTag, char* pVpdItem);
    virtual DTAPI_RESULT  VpdRead(const wchar_t* pTag, wchar_t* pVpdItem);
    virtual DTAPI_RESULT  VpdRead(const char*  pTag, char* pVpdItem, int& ItemSize);
    virtual DTAPI_RESULT  VpdRead(const wchar_t*  pTag, char* pVpdItem, int& ItemSize);
    virtual DTAPI_RESULT  VpdWrite(const char* pTag, char* pVpdItem);
    virtual DTAPI_RESULT  VpdWrite(const wchar_t* pTag, wchar_t* pVpdItem);
    virtual DTAPI_RESULT  VpdWrite(const char* pTag, char* pVpdItem, int ItemSize);
    virtual DTAPI_RESULT  VpdWrite(const wchar_t* pTag, char* pVpdItem, int ItemSize);

protected:
    virtual void  LoadDeviceData();
private:
    static void  DtEventCallback(int Event, DtEventArgs* pArgs);

    // Public attributes
public:
    DtDeviceDesc  m_DvcDesc;        // Device descriptor, initialized in attach
    DtHwFuncDesc*  m_pHwf;          // Hardware functions, initialized in attach
    
    // Implementation data
private:
    std::list<void*>  m_EventSubscriberList;

    // Friends
    friend class  DtInpChannel;
    friend class  DtOutpChannel;

public:                             // TODOSD should be protected
    IDevice*  m_pDev;
};

// Attribute identifiers
#define DTAPI_ATTR_LEVEL_MAX        1
#define DTAPI_ATTR_LEVEL_RANGE      2
#define DTAPI_ATTR_LEVEL_STEPSIZE   3
#define DTAPI_ATTR_RFFREQ_ABSMAX    4
#define DTAPI_ATTR_RFFREQ_ABSMIN    5
#define DTAPI_ATTR_RFFREQ_MAX       6
#define DTAPI_ATTR_RFFREQ_MIN       7
#define DTAPI_ATTR_SAMPRHW_ABSMAX   8
#define DTAPI_ATTR_SAMPRHW_ABSMIN   9
#define DTAPI_ATTR_SAMPRHW_HARDLIM  10
#define DTAPI_ATTR_SAMPRHW_MAX      11
#define DTAPI_ATTR_SAMPRHW_MIN      12
#define DTAPI_ATTR_SAMPRATE_ABSMAX  13
#define DTAPI_ATTR_SAMPRATE_ABSMIN  14
#define DTAPI_ATTR_SAMPRATE_MAX     15
#define DTAPI_ATTR_SAMPRATE_MIN     16
#define DTAPI_ATTR_NUM_FANS         17
#define DTAPI_ATTR_PCIE_REQ_BW      18
#define DTAPI_ATTR_PCIE_AVAIL_BW    19

// Order in which devices should be listed by DtapiDeviceScan/DtapiHwFuncScan
#define DTAPI_SCANORDER_ORIG        0    // Devices are returned in order determined by OS
#define DTAPI_SCANORDER_SN          1    // Devices are sorted by serial number

// String conversion - Device type number (e.g. "DTA-100", "DTA-102")
#define DTAPI_DVC2STR_TYPE_NMB      0
// String conversion - Device type number + location (e.g. "DTA-100 in slot 5");
#define DTAPI_DVC2STR_TYPE_AND_LOC  1
// String conversion - Device type number (e.g. "DTA-100", "DTA-102")
#define DTAPI_HWF2STR_TYPE_NMB      0
// String conversion - Device type number + port (e.g. "DTA-124 port 1")
#define DTAPI_HWF2STR_TYPE_AND_PORT 1
// String conversion - Device type number + location (e.g. "DTA-100 in slot 5");
#define DTAPI_HWF2STR_TYPE_AND_LOC  2
// String conversion - Interface type (e.g. "DVB-ASI" or "DVB-C")
#define DTAPI_HWF2STR_ITF_TYPE      3
// String conversion - Short version of interface type (e.g. "ASI" instead "DVB-ASI")
#define DTAPI_HWF2STR_ITF_TYPE_SHORT 4

// Current genlock state
#define DTAPI_GENL_NO_REF           1
#define DTAPI_GENL_LOCKING          2
#define DTAPI_GENL_LOCKED           3

// Constants for GetStateFlags() on port level
#define DTAPI_STATE_FLAG_INSUFF_USB_BW  0x010000
#define DTAPI_STATE_FLAG_SDI_NO_LOCK    0x020000
#define DTAPI_STATE_FLAG_SDI_INVALID    0x040000
// Constants for GetStateFlags() on device level
#define DTAPI_STATE_FLAG_VPD_CORRUPT    0x000001
#define DTAPI_STATE_FLAG_NO_SERIAL      0x000002
#define DTAPI_STATE_FLAG_NO_USB3        0x000004
#define DTAPI_STATE_FLAG_SLEEPING       0x000008

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDtaPlusDevice -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class representing a DekTec DTA-plus Device
//
class DtDtaPlusDevice
{
    // Constructor, destructor
public:
    DtDtaPlusDevice();
    virtual ~DtDtaPlusDevice();
private:
    // No implementation is provided for the copy constructor
    DtDtaPlusDevice(const DtDtaPlusDevice&);

    // Public access functions
public:
    bool  IsAttached(void);

    // Public member functions
public:
    DTAPI_RESULT  AttachToDevice(const DtDtaPlusDeviceDesc &DvcDesc);
    DTAPI_RESULT  AttachToSerial(__int64 SerialNumber);
    DTAPI_RESULT  Detach();
    DTAPI_RESULT  GetDeviceStatus(int &Status); 
    DTAPI_RESULT  GetTempControlStatus(int &ControlStatus);
    DTAPI_RESULT  GetTemperature(int &Temperature);
    DTAPI_RESULT  GetSerialNumber(__int64 &SerialNumber);
    DTAPI_RESULT  SetRfOutLevel(int Level);
    DTAPI_RESULT  SetFreq(int Freq);

private:
    DtaPlusDevice*  m_Dev;
};

// DTA-plus status codes
#define DTAPI_DTAPLUS_STATUS_OFF              0  // DTA-Plus not ready for operation
#define DTAPI_DTAPLUS_STATUS_ON               1  // DTA-Plus ready for operation
#define DTAPI_DTAPLUS_STATUS_ATTN_FOLLOW_UP   2  // DTA-Plus attenuator ctrl following up
#define DTAPI_DTAPLUS_STATUS_ATTN_FOLLOW_DOWN 3  //DTA-Plus attenuator ctrl following down
#define DTAPI_DTAPLUS_STATUS_DAC_FOLLOW_UP    4  // DTA-Plus DAC control following up
#define DTAPI_DTAPLUS_STATUS_DAC_FOLLOW_DOWN  5  // DTA-Plus DAC control following down
#define DTAPI_DTAPLUS_STATUS_HOLD             6  // DTA-Plus has valid input signal
#define DTAPI_DTAPLUS_STATUS_NO_SIGNAL        7  // DTA-Plus has no input signal
#define DTAPI_DTAPLUS_STATUS_OVER_POWER       8  // DTA-Plus input signal is too high

// DTA-plus temperature control states
#define DTAPI_DTAPLUS_TEMP_CONTROL_OFF        0  // DTA-Plus temperature control is off
#define DTAPI_DTAPLUS_TEMP_CONTROL_FAN_ON     1  // DTA-Plus fan is on
#define DTAPI_DTAPLUS_TEMP_CONTROL_HEATER_ON  2  // DTA-Plus heater is on

// Ethernet speed
#define DTAPI_NWSPEED_AUTO          0            // Set
#define DTAPI_NWSPEED_NOLIN         0            // Get
#define DTAPI_NWSPEED_10MB_HALF     1
#define DTAPI_NWSPEED_10MB_FULL     2
#define DTAPI_NWSPEED_100MB_HALF    3
#define DTAPI_NWSPEED_100MB_FULL    4
#define DTAPI_NWSPEED_1GB_MASTER    5
#define DTAPI_NWSPEED_1GB_SLAVE     6

// Microcode upload states
#define DTAPI_UCODE_NOT_LOADED      0
#define DTAPI_UCODE_LOADING         1
#define DTAPI_UCODE_LOADED          2

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtInpChannel -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class to represent an input channel
//
class  DtInpChannel
{
public:
    DtInpChannel();
    virtual ~DtInpChannel();
private:
    // No implementation is provided for the copy constructor
    DtInpChannel(const DtInpChannel&);

public:
    DtHwFuncDesc  m_HwFuncDesc;     // Hardware function descriptor

    // Convenience functions
public:
    int  Category(void)         { return m_HwFuncDesc.m_DvcDesc.m_Category; }
    int  FirmwareVersion(void)  { return m_HwFuncDesc.m_DvcDesc.m_FirmwareVersion; }
    bool  IsAttached(void)      { return m_pInp != NULL; }
    int  TypeNumber(void)       { return m_HwFuncDesc.m_DvcDesc.m_TypeNumber; }
    bool  HasCaps(const DtCaps  Caps) const 
    { 
        return ((m_HwFuncDesc.m_Flags & Caps) == Caps); 
    }

public:
    DTAPI_RESULT  AttachToPort(DtDevice* pDtDvc, int Port,
                                               bool Exclusive=true, bool ProbeOnly=false);
    DTAPI_RESULT  BlindScan(int NumEntries, int& NumEntriesResult, 
                         DtTransmitter* pScanResults, __int64 FreqHzSteps=10000000LL, 
                         __int64 StartFreqHz=950000000LL, __int64 EndFreqHz=2150000000LL);
    DTAPI_RESULT  ClearFifo();
    DTAPI_RESULT  ClearFlags(int Latched);
    DTAPI_RESULT  Detach(int DetachMode);
    DTAPI_RESULT  DetectIoStd(int& Value, int& SubValue);
    DTAPI_RESULT  Equalise(int EqualiserSetting);
    DTAPI_RESULT  GetConstellationPoints(int NumPoints, DtConstelPoint* pPoint);
    DTAPI_RESULT  GetDemodControl(int& ModType,
                                             int& ParXtra0, int& ParXtra1, int& ParXtra2);
    DTAPI_RESULT  GetDemodControl(DtDemodPars* pDemodPars);
    DTAPI_RESULT  GetDescriptor(DtHwFuncDesc& HwFunDesc);
    DTAPI_RESULT  GetFifoLoad(int& FifoLoad);
    DTAPI_RESULT  GetFlags(int& Flags, int& Latched);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue, __int64& ParXtra0);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue,
                                                    __int64& ParXtra0, __int64& ParXtra1);
    DTAPI_RESULT  GetIpPars(DtIpPars* pIpPars);
    DTAPI_RESULT  GetIpStat(DtIpStat* pIpStat);
    DTAPI_RESULT  GetMaxFifoSize(int& MaxFifoSize);
    DTAPI_RESULT  GetPars(int Count, DtPar* pPars);    
    DTAPI_RESULT  GetRxClkFreq(int& RxClkFreq);
    DTAPI_RESULT  GetRxControl(int& RxControl);
    DTAPI_RESULT  GetRxMode(int& RxMode);
    DTAPI_RESULT  GetStatistics(int Count, DtStatistic* pStatistics);
    DTAPI_RESULT  GetStatistic(int Type, int& Statistic);
    DTAPI_RESULT  GetStatistic(int Type, double& Statistic);
    DTAPI_RESULT  GetStatistic(int Type, bool& Statistic);
    DTAPI_RESULT  GetStatus(int& PacketSize, int& NumInv, int& ClkDet,
                                                  int& AsiLock, int& RateOk, int& AsiInv);
    DTAPI_RESULT  GetStreamSelection(DtDabEtiStreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtDabStreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtDvbC2StreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtDvbTStreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtDvbT2StreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtIsdbtStreamSelPars& StreamSel);
    DTAPI_RESULT  GetStreamSelection(DtT2MiStreamSelPars& StreamSel);
    DTAPI_RESULT  GetSupportedStatistics(int& Count, DtStatistic* pStatistics);
    DTAPI_RESULT  GetTargetId(int& Present, int& TargetId);
    DTAPI_RESULT  GetTsRateBps(int& TsRate);
    DTAPI_RESULT  GetTunerFrequency(__int64& FreqHz, int TunerId=-1);
    DTAPI_RESULT  GetViolCount(int& ViolCount);
    DTAPI_RESULT  I2CLock(int TimeOut);
    DTAPI_RESULT  I2CUnlock(void);
    DTAPI_RESULT  I2CRead(int DvcAddr, char* pBuffer, int NumBytesToRead);
    DTAPI_RESULT  I2CWrite(int DvcAddr, char* pBuffer, int NumBytesToWrite);
    DTAPI_RESULT  I2CWriteRead(int DvcAddrWrite, char* pBufferWrite, int NumBytesToWrite,
                                  int DvcAddrRead, char* pBufferRead, int NumBytesToRead);
    DTAPI_RESULT  LedControl(int LedControl);
    DTAPI_RESULT  LnbEnable(bool Enable);
    DTAPI_RESULT  LnbEnableTone(bool Enable);
    DTAPI_RESULT  LnbSetVoltage(int Level);
    DTAPI_RESULT  LnbSendBurst(int BurstType);
    DTAPI_RESULT  LnbSendDiseqcMessage(const unsigned char* MsgOut, int NumBytesOut);
    DTAPI_RESULT  LnbSendDiseqcMessage(const unsigned char* MsgOut, int NumBytesOut, 
                                                   unsigned char* MsgIn, int& NumBytesIn);
    DTAPI_RESULT  PolarityControl(int Polarity);
    DTAPI_RESULT  Read(char* pBuffer, int NumBytesToRead);
    DTAPI_RESULT  Read(char* pBuffer, int NumBytesToRead, int TimeOut);
    DTAPI_RESULT  ReadFrame(unsigned int* pFrame, int& FrameSize, int TimeOut=-1);
    DTAPI_RESULT  RegisterDemodCallback(IDtDemodEvent* pIEvent, __int64 Events=-1);
    DTAPI_RESULT  Reset(int ResetMode);
    DTAPI_RESULT  SetAdcSampleRate(int SampleRate);
    DTAPI_RESULT  SetAntPower(int AntPower);
    DTAPI_RESULT  SetDemodControl(int ModType, int ParXtra0, int ParXtra1, int ParXtra2);
    DTAPI_RESULT  SetDemodControl(DtDemodPars *pDemodPars);
    DTAPI_RESULT  SetErrorStatsMode(int ModType, int Mode);
    DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    DTAPI_RESULT  SetIpPars(DtIpPars* pIpPars);
    DTAPI_RESULT  SetPars(int Count, DtPar* pPars); 
    DTAPI_RESULT  SetRxControl(int RxControl);
    DTAPI_RESULT  SetRxMode(int RxMode);
    DTAPI_RESULT  SetStreamSelection(DtDabEtiStreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtDabStreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtDvbC2StreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtDvbTStreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtDvbT2StreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtIsdbtStreamSelPars& StreamSel);
    DTAPI_RESULT  SetStreamSelection(DtT2MiStreamSelPars& StreamSel);
    DTAPI_RESULT  SetTuningMode(int  Mode);
    DTAPI_RESULT  SetTunerFrequency(__int64 FreqHz, int TunerId=-1);
    DTAPI_RESULT  Tune(__int64 FreqHz, int ModType,
                                                int ParXtra0, int ParXtra1, int ParXtra2);
    DTAPI_RESULT  Tune(__int64 FreqHz, DtDemodPars *pDemodPars);

    // Encapsulated data
private:
    IXpMutex*  m_pMTLock;           // Multi-threading lock for Get/Read functions
    void*  m_pDetachLockCount;
    int  m_Port;
    bool  m_WantToDetach;

public:                             // TODOSD should be protected
    InpChannel*  m_pInp;            // Input channel implementation
    
// Private helper functions
private:
    DTAPI_RESULT  DetachLock(void);
    DTAPI_RESULT  DetachUnlock(void);
    DTAPI_RESULT  ReadAccessLock(void);
    DTAPI_RESULT  ReadAccessUnlock(void);
    DTAPI_RESULT  ReadWithTimeOut(char* pBuf, int NumBytesToRead, int TimeOut = -1);
};

// Tuner freq has changed
#define DTAPI_EV_TUNE_FREQ_CHANGED  0x0000000000000001LL
// Tuning parameters have changed
#define DTAPI_EV_TUNE_PARS_CHANGED  0x0000000000000002LL

#define DTAPI_ERRORSTATS_BER        0            // Bit error rate (default)
#define DTAPI_ERRORSTATS_RS         1            // Reed-Solomon packet errors

// Feature not supported
#define DTAPI_NOT_SUPPORTED         -1

// ASI Polarity-Control Status
#define DTAPI_ASIINV_NORMAL         0
#define DTAPI_ASIINV_INVERT         1

// ASI Input-Clock Lock
#define DTAPI_ASI_NOLOCK            0
#define DTAPI_ASI_INLOCK            1

// SDI Input-Clock Lock
#define DTAPI_GENLOCK_NOLOCK        0
#define DTAPI_GENLOCK_INLOCK        1

// Clock Detector
#define DTAPI_CLKDET_FAIL           0
#define DTAPI_CLKDET_OK             1

// Input Rate Ok
#define DTAPI_INPRATE_LOW           0
#define DTAPI_INPRATE_OK            1

// #Invalid bytes per packet
#define DTAPI_NUMINV_NONE           0
#define DTAPI_NUMINV_16             1
#define DTAPI_NUMINV_OTHER          2

// Packet Size
#define DTAPI_PCKSIZE_INV           0
#define DTAPI_PCKSIZE_188           2
#define DTAPI_PCKSIZE_204           3

// SDI Mode
#define DTAPI_SDIMODE_INV           0
#define DTAPI_SDIMODE_525           1
#define DTAPI_SDIMODE_625           2

// Receive Control
#define DTAPI_RXCTRL_IDLE           0
#define DTAPI_RXCTRL_RCV            1

// Receive mode for Transport Streams - Modes
#define DTAPI_RXMODE_TS             0x10
#define DTAPI_RXMODE_TS_MODE_BITS   0x0F
#define DTAPI_RXMODE_ST188          (DTAPI_RXMODE_TS | 0x01)
#define DTAPI_RXMODE_ST204          (DTAPI_RXMODE_TS | 0x02)
#define DTAPI_RXMODE_STMP2          (DTAPI_RXMODE_TS | 0x03)
#define DTAPI_RXMODE_STRAW          (DTAPI_RXMODE_TS | 0x04)
#define DTAPI_RXMODE_STL3           (DTAPI_RXMODE_TS | 0x05)
#define DTAPI_RXMODE_STL3FULL       (DTAPI_RXMODE_TS | 0x06)
#define DTAPI_RXMODE_IPRAW          (DTAPI_RXMODE_TS | 0x07)
#define DTAPI_RXMODE_RAWASI         (DTAPI_RXMODE_TS | 0x08)
#define DTAPI_RXMODE_STTRP          (DTAPI_RXMODE_TS | 0x09)
#define DTAPI_RXMODE_TS_MASK        (DTAPI_RXMODE_TS | DTAPI_RXMODE_TS_MODE_BITS)

// Receive mode for SDI - Modes
#define DTAPI_RXMODE_SDI            0x1000
#define DTAPI_RXMODE_SDI_MODE_BITS  0x0F00
#define DTAPI_RXMODE_SDI_FULL       (DTAPI_RXMODE_SDI | 0x100)
#define DTAPI_RXMODE_SDI_ACTVID     (DTAPI_RXMODE_SDI | 0x200)
#define DTAPI_RXMODE_SDI_MASK       (DTAPI_RXMODE_SDI | DTAPI_RXMODE_SDI_MODE_BITS)
// Receive mode for SDI - Flags
#define DTAPI_RXMODE_SDI_HUFFMAN    0x00002000
#define DTAPI_RXMODE_SDI_10B        0x00004000
#define DTAPI_RXMODE_SDI_16B        0x00008000
#define DTAPI_RXMODE_SDI_10B_NBO    0x00010000

// Receive mode for SDI and Transport Streams - Common flags
#define DTAPI_RXMODE_TIMESTAMP32    0x01000000
#define DTAPI_RXMODE_TIMESTAMP64    0x02000000

// Demodulation status flags - FEC lock
#define DTAPI_DEMOD_FECLOCK_FAIL    0
#define DTAPI_DEMOD_FECLOCK_OK      1
// Demodulation status flags - Receiver lock
#define DTAPI_DEMOD_RCVLOCK_FAIL    0
#define DTAPI_DEMOD_RCVLOCK_OK      1    

// Channel bands
#define DTAPI_BAND_BROADCAST_ONAIR  1
#define DTAPI_BAND_FCC_CABLE        2
#define DTAPI_BAND_IRC              3
#define DTAPI_BAND_HRC              4

// RF level bandwith
#define DTAPI_RFLVL_CHANNEL         0
#define DTAPI_RFLVL_NARROWBAND      1

// ADC sampling rates
#define DTAPI_ADCCLK_OFF            0            // Clock is off
#define DTAPI_ADCCLK_20M647         20647059     // 20.647059 MHz clock
#define DTAPI_ADCCLK_13M5           13500000     // 13.5 MHz clock
#define DTAPI_ADCCLK_27M            27000000     // 27.0 MHz clock

// LNB control values
#define DTAPI_LNB_13V               0            // LNB power 13V
#define DTAPI_LNB_18V               1            // LNB power 18V
#define DTAPI_LNB_14V               2            // LNB power 14V
#define DTAPI_LNB_19V               3            // LNB power 19V

// LNB burst types
#define DTAPI_LNB_BURST_A           0            // Burst A
#define DTAPI_LNB_BURST_B           1            // Burst B

// Tuner Parameters - Tuner standard
#define DTAPI_TUNMOD_QAM            0x1
#define DTAPI_TUNMOD_ATSC           0x2
#define DTAPI_TUNMOD_ISDBT          0x3
#define DTAPI_TUNMOD_DVBT           0x4
#define DTAPI_TUNMOD_DMBT           0x5

// Tuner Parameters - DTA-2131 specific - Value for automatic computation of parameters
#define DTAPI_TUN31_AUTO            -1           // According to tuner standard

// Tuner Parameters - DTA-2131 specific - Low-pass filter cutoff frequency
#define DTAPI_TUN31_LPF_1_5MHZ      0            // 1.5 MHz low-pass filter
#define DTAPI_TUN31_LPF_6MHZ        1            // 6 MHz low-pass filter
#define DTAPI_TUN31_LPF_7MHZ        2            // 7 MHz low-pass filter
#define DTAPI_TUN31_LPF_8MHZ        3            // 8 MHz low-pass filter  
#define DTAPI_TUN31_LPF_9MHZ        4            // 9 MHz low-pass filter   

// Tuner Parameters - DTA-2131 specific -  Low-pass filter offset 
#define DTAPI_TUN31_LPF_0PCT        0            // 0% low-pass filter offset  
#define DTAPI_TUN31_LPF_4PCT        1            // 4% low-pass filter offset  
#define DTAPI_TUN31_LPF_8PCT        2            // 8% low-pass filter offset  
#define DTAPI_TUN31_LPF_12PCT       3            // 12% low-pass filter offset

// Tuner Parameters - DTA-2131 specific - IF hi-pass filter 
#define DTAPI_TUN31_HPF_DIS         0            // Disabled IF hi-pass filter 
#define DTAPI_TUN31_HPF_0_4MHZ      1            // 0.4 MHz IF hi-pass filter  
#define DTAPI_TUN31_HPF_0_85MHZ     2            // 0.85 MHz IF hi-pass filter
#define DTAPI_TUN31_HPF_1MHZ        3            // 1 MHz IF hi-pass filter
#define DTAPI_TUN31_HPF_1_5MHZ      4            // 1.5 MHz IF hi-pass filter

// Tuner Parameters - DTA-2131 specific - Notch settings
#define DTAPI_TUN31_NOTCH_DIS       0            // Disable
#define DTAPI_TUN31_NOTCH_ENA       1            // Enable

// Tuner Parameters - DTA-2139 specific - Agc specific
#define DTAPI_AGC1_FREE             0
#define DTAPI_AGC1_FROZEN           1

// Tuning mode - DTU-236A specific
#define DTAPI_TUNING_NORMAL         0           // Standard tuning mode
#define DTAPI_TUNING_INDEPENDENT    1           // Multiple tuners, tuned independently

// Tuner ID - DTU-236A specific
#define DTAPI_TUNERID_ALL           -1          // ID for all tuners
#define DTAPI_TUNERID_MAIN          0           // ID for main tuner
#define DTAPI_TUNERID_MEASUREMENT   1           // ID for measurement tuner

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtOutpChannel -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class to represent a transport-stream or SDI output channel
//
class DtOutpChannel
{
public:
    DtOutpChannel();
    virtual ~DtOutpChannel();
private:
    // No implementation is provided for the copy constructor
    DtOutpChannel(const DtOutpChannel&);

public:
    DtHwFuncDesc  m_HwFuncDesc;     // Hardware function descriptor

    // Convenience functions
public:
    int  Category(void)         { return m_HwFuncDesc.m_DvcDesc.m_Category; }
    int  FirmwareVersion(void)  { return m_HwFuncDesc.m_DvcDesc.m_FirmwareVersion; }
    bool  IsAttached(void)      { return m_pOutp != NULL; }
    int  TypeNumber(void)       { return m_HwFuncDesc.m_DvcDesc.m_TypeNumber; }
    bool  HasCaps(const DtCaps  Caps) const 
    { 
        return ((m_HwFuncDesc.m_Flags & Caps) == Caps); 
    }

public:
    virtual DTAPI_RESULT  AttachToPort(DtDevice* pDtDvc, int Port, bool ProbeOnly=false);
    virtual DTAPI_RESULT  ClearFifo(void);
    virtual DTAPI_RESULT  ClearFlags(int Latched);
    virtual DTAPI_RESULT  Detach(int DetachMode);
    virtual DTAPI_RESULT  GetAttribute(int AttrId, int& AttrValue);
    virtual DTAPI_RESULT  GetAttribute(int AttrId, DtModPars& ModParVals, int& AttrValue);
    virtual DTAPI_RESULT  GetDescriptor(DtHwFuncDesc& HwFunDesc);
    virtual DTAPI_RESULT  GetExtClkFreq(int& ExtClkFreq);
    virtual DTAPI_RESULT  GetFailsafeAlive(bool& Alive);
    virtual DTAPI_RESULT  GetFailsafeConfig(bool& Enable, int& Timeout);
    virtual DTAPI_RESULT  GetFifoLoad(int& FifoLoad);
    virtual DTAPI_RESULT  GetFifoSize(int& FifoSize);
    virtual DTAPI_RESULT  GetFifoSizeMax(int& FifoSizeMax);
    virtual DTAPI_RESULT  GetFifoSizeTyp(int& FifoSizeTyp);
    virtual DTAPI_RESULT  GetFlags(int& Status, int& Latched);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue,
                                                                       __int64& ParXtra0);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue,
                                                    __int64& ParXtra0, __int64& ParXtra1);
    virtual DTAPI_RESULT  GetIpPars(DtIpPars* pIpPars);
    virtual DTAPI_RESULT  GetMaxFifoSize(int& MaxFifoSize);
    virtual DTAPI_RESULT  GetModControl(int& ModType, int& ParXtra0, int& ParXtra1,
                                                         int& ParXtra2, void*& pXtraPars);
    virtual DTAPI_RESULT  GetOutputLevel(int& LeveldBm);
    virtual DTAPI_RESULT  GetRfControl(__int64& RfFreq, int& LockStatus);
    virtual DTAPI_RESULT  GetSpiClk(int& SpiClk);
    virtual DTAPI_RESULT  GetTargetId(int& Present, int& TargetId);
    virtual DTAPI_RESULT  GetTsRateBps(int& TsRate);
    virtual DTAPI_RESULT  GetTxControl(int& TxControl);
    virtual DTAPI_RESULT  GetTxMode(int& TxMode, int& TxStuffMode);
    virtual DTAPI_RESULT  LedControl(int LedControl);
    virtual DTAPI_RESULT  Reset(int ResetMode);
    virtual DTAPI_RESULT  SetChannelModelling(bool CmEnable, DtCmPars& CmPars);    
    virtual DTAPI_RESULT  SetCustomRollOff(bool Enable, DtFilterPars& Filter); 
    virtual DTAPI_RESULT  SetFailsafeAlive();
    virtual DTAPI_RESULT  SetFailsafeConfig(bool Enable, int Timeout = 0);
    virtual DTAPI_RESULT  SetFifoSize(int FifoSize);
    virtual DTAPI_RESULT  SetFifoSizeMax(void);
    virtual DTAPI_RESULT  SetFifoSizeTyp(void);
    virtual DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    virtual DTAPI_RESULT  SetIpPars(DtIpPars* pIpPars);
    virtual DTAPI_RESULT  SetIsdbtCaptFile(void* IsdbtFile);
    virtual DTAPI_RESULT  SetModControl(DtCmmbPars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbC2Pars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbS2Pars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbT2Pars&);
    virtual DTAPI_RESULT  SetModControl(DtIsdbsPars&);
    virtual DTAPI_RESULT  SetModControl(DtIsdbtPars&);
    virtual DTAPI_RESULT  SetModControl(DtIsdbTmmPars&);
    virtual DTAPI_RESULT  SetModControl(int ModType, int ParXtra0, int ParXtra1, 
                                                                            int ParXtra2);
    virtual DTAPI_RESULT  SetModControl(unsigned char*);
    virtual DTAPI_RESULT  SetOutputLevel(int LeveldBm);
    virtual DTAPI_RESULT  SetPower(int Power);
    virtual DTAPI_RESULT  SetRfControl(__int64 RfFreq);
    virtual DTAPI_RESULT  SetRfMode(int RfMode);
    virtual DTAPI_RESULT  SetRfMode(int Sel, int Mode);
    virtual DTAPI_RESULT  SetSnr(int Mode, int Snr);
    virtual DTAPI_RESULT  SetSpiClk(int SpiClk);
    virtual DTAPI_RESULT  SetTsRateBps(int TsRate);
    virtual DTAPI_RESULT  SetTsRateRatio(int TsRate, int ClockRef);
    virtual DTAPI_RESULT  SetTxControl(int TxControl);
    virtual DTAPI_RESULT  SetTxMode(int TxMode, int StuffMode);
    virtual DTAPI_RESULT  SetTxPolarity(int TxPolarity);
    virtual DTAPI_RESULT  Write(char* pBuffer, int NumBytesToWrite);
    // Undocumented
    virtual DTAPI_RESULT  GetModBufLoads(bool&, int&, int&, int&);

public:                             // TODOSD should be protected
    OutpChannel*  m_pOutp;          // Output channel implementation

private:
    void*  m_pDetachLockCount;
    bool  m_WantToDetach;
    DTAPI_RESULT  DetachLock(void);
    DTAPI_RESULT  DetachUnlock(void);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtMplpOutpChannel -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
class DtMplpOutpChannel : public DtOutpChannel
{
public:
    DtMplpOutpChannel();
    virtual ~DtMplpOutpChannel();
private:
    // No implementation is provided for the copy constructor
    DtMplpOutpChannel(const DtMplpOutpChannel&);

public:
    virtual bool IsAttached(void);

public:
    virtual DTAPI_RESULT  AttachToPort(DtDevice* pDtDvc, int Port, bool ProbeOnly=false);
    virtual DTAPI_RESULT  ClearFifo(void);
    virtual DTAPI_RESULT  ClearFlags(int Latched);
    virtual DTAPI_RESULT  Detach(int DetachMode);
    virtual DTAPI_RESULT  GetAttribute(int AttrId, int& AttrValue);
    virtual DTAPI_RESULT  GetAttribute(int AttrId, DtModPars& ModParVals, int& AttrValue);
    virtual DTAPI_RESULT  GetDescriptor(DtHwFuncDesc& HwFunDesc);
    virtual DTAPI_RESULT  GetExtClkFreq(int& ExtClkFreq);
    virtual DTAPI_RESULT  GetFailsafeAlive(bool& Alive);
    virtual DTAPI_RESULT  GetFailsafeConfig(bool& Enable, int& Timeout);
    virtual DTAPI_RESULT  GetFifoLoad(int& FifoLoad);
    virtual DTAPI_RESULT  GetFifoSize(int& FifoSize);
    virtual DTAPI_RESULT  GetFifoSizeMax(int& FifoSizeMax);
    virtual DTAPI_RESULT  GetFifoSizeTyp(int& FifoSizeTyp);
    virtual DTAPI_RESULT  GetFlags(int& Status, int& Latched);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue,
                                                                       __int64& ParXtra0);
    virtual DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue, 
                                                    __int64& ParXtra0, __int64& ParXtra1);
    virtual DTAPI_RESULT  GetIpPars(DtIpPars* pIpPars);
    virtual DTAPI_RESULT  GetMaxFifoSize(int& MaxFifoSize);
    virtual DTAPI_RESULT  GetModControl(int& ModType, int& CodeRate,
                                          int& ParXtra1, int& ParXtra2, void*& pXtraPars);
    virtual DTAPI_RESULT  GetOutputLevel(int& LeveldBm);
    virtual DTAPI_RESULT  GetRfControl(__int64& RfFreq, int& LockStatus);
    virtual DTAPI_RESULT  GetSpiClk(int& SpiClk);
    virtual DTAPI_RESULT  GetTargetId(int& Present, int& TargetId);
    virtual DTAPI_RESULT  GetTsRateBps(int& TsRate);
    virtual DTAPI_RESULT  GetTxControl(int& TxControl);
    virtual DTAPI_RESULT  GetTxMode(int& TxMode, int& TxStuffMode);
    virtual DTAPI_RESULT  LedControl(int LedControl);
    virtual DTAPI_RESULT  Reset(int ResetMode);
    virtual DTAPI_RESULT  SetChannelModelling(bool CmEnable, DtCmPars& CmPars);    
    virtual DTAPI_RESULT  SetCustomRollOff(bool Enable, DtFilterPars& Filter); 

    virtual DTAPI_RESULT  SetFailsafeConfig(bool Enable, int Timeout = 0);
    virtual DTAPI_RESULT  SetFailsafeAlive();
    virtual DTAPI_RESULT  SetFifoSize(int FifoSize);
    virtual DTAPI_RESULT  SetFifoSizeMax(void);
    virtual DTAPI_RESULT  SetFifoSizeTyp(void);
    virtual DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    virtual DTAPI_RESULT  SetIpPars(DtIpPars* pIpPars);
    virtual DTAPI_RESULT  SetIsdbtCaptFile(void* IsdbtFile);
    virtual DTAPI_RESULT  SetModControl(DtIsdbsPars&);
    virtual DTAPI_RESULT  SetModControl(DtIsdbtPars&);
    virtual DTAPI_RESULT  SetModControl(DtIsdbTmmPars&);
    virtual DTAPI_RESULT  SetModControl(DtCmmbPars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbC2Pars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbS2Pars&);
    virtual DTAPI_RESULT  SetModControl(DtDvbT2Pars&);
    virtual DTAPI_RESULT  SetModControl(int ModType, int ParXtra0, int ParXtra1, 
                                                                            int ParXtra2);
    virtual DTAPI_RESULT  SetModControl(unsigned char*);
    virtual DTAPI_RESULT  SetOutputLevel(int LeveldBm);
    virtual DTAPI_RESULT  SetPower(int Power);
    virtual DTAPI_RESULT  SetRfControl(__int64 RfFreq);
    virtual DTAPI_RESULT  SetRfMode(int RfMode);
    virtual DTAPI_RESULT  SetRfMode(int Sel, int Mode);
    virtual DTAPI_RESULT  SetSnr(int Mode, int Snr);
    virtual DTAPI_RESULT  SetSpiClk(int SpiClk);
    virtual DTAPI_RESULT  SetTsRateBps(int TsRate);
    virtual DTAPI_RESULT  SetTsRateRatio(int TsRate, int ClockRef);
    virtual DTAPI_RESULT  SetTxControl(int TxControl);
    virtual DTAPI_RESULT  SetTxMode(int TxMode, int TxStuffMode);
    virtual DTAPI_RESULT  SetTxPolarity(int TxPolarity);
    virtual DTAPI_RESULT  Write(char* pBuffer, int NumBytesToWrite);
    // Undocumented
    virtual DTAPI_RESULT  GetModBufLoads(bool&, int&, int&, int&);

    // IMplpModulator interface implementation
public:
    virtual DTAPI_RESULT  AttachVirtual(DtDevice* pDtDvc, 
                                  bool (*pFunc)(void*, DtVirtualOutData*), void* pOpaque);
    virtual DTAPI_RESULT  GetMplpFifoFree(int FifoIdx, int& FifoFree);
    virtual DTAPI_RESULT  GetMplpFifoLoad(int FifoIdx, int& FifoLoad);
    virtual DTAPI_RESULT  GetMplpFifoSize(int FifoIdx, int& FifoSize);
    virtual DTAPI_RESULT  GetMplpModStatus(DtDvbC2ModStatus* pMplpModStat);
    virtual DTAPI_RESULT  GetMplpModStatus(DtDvbS2ModStatus* pMplpModStat);
    virtual DTAPI_RESULT  GetMplpModStatus(DtDvbT2ModStatus* pMplpModStat);
    virtual DTAPI_RESULT  GetMplpModStatus(DtDvbT2ModStatus* pMplpModStat1,
                                                         DtDvbT2ModStatus* pMplpModStat2);
    virtual DTAPI_RESULT  SetMplpChannelModelling(bool CmEnable, DtCmPars&, int Chan=0);
    virtual DTAPI_RESULT  WriteMplp(int FifoIdx, char* pBuffer, int NumBytesToWrite);
    virtual DTAPI_RESULT  WriteMplpPacket(int FifoIdx, char* pPacket, int PacketSize);

private:
    bool  m_IsAttachedToVirtual;
    MplpHelper*  m_pMplpHelper;
};

// Detach mode flags
#define DTAPI_INSTANT_DETACH        1
#define DTAPI_WAIT_UNTIL_SENT       2

// Equaliser settings 
#define DTAPI_EQUALISER_OFF         0
#define DTAPI_EQUALISER_ON          1

// LED control
#define DTAPI_LED_OFF               0
#define DTAPI_LED_GREEN             1
#define DTAPI_LED_RED               2
#define DTAPI_LED_YELLOW            3
#define DTAPI_LED_BLUE              4
#define DTAPI_LED_HARDWARE          5

// Noise modes
#define DTAPI_NOISE_DISABLED        0            // No noise generation
#define DTAPI_NOISE_WNG_HW          1            // White noise generator (hardware)

// Polarity control
#define DTAPI_POLARITY_AUTO         0
#define DTAPI_POLARITY_NORMAL       2
#define DTAPI_POLARITY_INVERT       3

// Power mode
#define DTAPI_POWER_OFF             0
#define DTAPI_POWER_ON              1

// Reset mode
#define DTAPI_FIFO_RESET            0
#define DTAPI_FULL_RESET            1

// RF PLL lock status
#define DTAPI_RFPLL_LOCK1           1            // RF PLL #1 is in lock
#define DTAPI_RFPLL_LOCK2           2            // RF PLL #2 is in lock
#define DTAPI_RFPLL_LOCK3           4            // RF PLL #3 is in lock

// Status flags
#define DTAPI_RX_FIFO_OVF           0x0002
#define DTAPI_RX_SYNC_ERR           0x0004
#define DTAPI_RX_RATE_OVF           0x0008
#define DTAPI_RX_TARGET_ERR         0x0010
#define DTAPI_RX_LINK_ERR           0x0040
#define DTAPI_RX_DATA_ERR           0x0080
#define DTAPI_RX_DRV_BUF_OVF        0x0100
#define DTAPI_RX_SYNTAX_ERR         0x0200

#define DTAPI_TX_FIFO_UFL           0x0002
#define DTAPI_TX_SYNC_ERR           0x0004
#define DTAPI_TX_READBACK_ERR       0x0008
#define DTAPI_TX_TARGET_ERR         0x0010
#define DTAPI_TX_MUX_OVF            0x0020
#define DTAPI_TX_FIFO_OVF           0x0020
#define DTAPI_TX_LINK_ERR           0x0040
#define DTAPI_TX_DATA_ERR           0x0080
#define DTAPI_TX_CPU_UFL            0x0100
#define DTAPI_TX_DMA_UFL            0x0200

// Target adapter present
#define DTAPI_NO_CONNECTION         0
#define DTAPI_DVB_SPI_SINK          1            // For output channels
#define DTAPI_DVB_SPI_SOURCE        1            // For input channels
#define DTAPI_TARGET_PRESENT        2
#define DTAPI_TARGET_UNKNOWN        3

// Transmit control
#define DTAPI_TXCTRL_IDLE           1
#define DTAPI_TXCTRL_HOLD           2
#define DTAPI_TXCTRL_SEND           3

// Transmit mode for Transport Streams - Modes
#define DTAPI_TXMODE_TS             0x10
#define DTAPI_TXMODE_TS_MODE_BITS   0x0F
#define DTAPI_TXMODE_188            (DTAPI_TXMODE_TS | 0x01)
#define DTAPI_TXMODE_192            (DTAPI_TXMODE_TS | 0x02)
#define DTAPI_TXMODE_204            (DTAPI_TXMODE_TS | 0x03)
#define DTAPI_TXMODE_ADD16          (DTAPI_TXMODE_TS | 0x04)
#define DTAPI_TXMODE_MIN16          (DTAPI_TXMODE_TS | 0x05)
#define DTAPI_TXMODE_IPRAW          (DTAPI_TXMODE_TS | 0x06)
#define DTAPI_TXMODE_RAW            (DTAPI_TXMODE_TS | 0x07)
#define DTAPI_TXMODE_RAWASI         (DTAPI_TXMODE_TS | 0x08)
#define DTAPI_TXMODE_TS_MASK        (DTAPI_TXMODE_TS | DTAPI_TXMODE_TS_MODE_BITS)
// Transmit mode for Transport Streams - DVB-ASI flags
#define DTAPI_TXMODE_BURST          0x20
#define DTAPI_TXMODE_TXONTIME       0x40

// Transmit mode for SDI - Modes
#define DTAPI_TXMODE_SDI            0x1000
#define DTAPI_TXMODE_SDI_MODE_BITS  0x0F00
#define DTAPI_TXMODE_SDI_FULL       (DTAPI_TXMODE_SDI | 0x100)
#define DTAPI_TXMODE_SDI_ACTVID     (DTAPI_TXMODE_SDI | 0x200)
#define DTAPI_TXMODE_SDI_MASK       (DTAPI_TXMODE_SDI | DTAPI_TXMODE_SDI_MODE_BITS)
// Transmit mode for SDI - Flags
#define DTAPI_TXMODE_SDI_HUFFMAN    0x00002000
#define DTAPI_TXMODE_SDI_10B        0x00004000
#define DTAPI_TXMODE_SDI_16B        0x00008000
#define DTAPI_TXMODE_SDI_10B_NBO    0x00010000

// Stuff mode - TS : Null-packet stuffing on/off; SDI: Black-frame stuffing on/off
#define DTAPI_TXSTUFF_MODE_OFF      0
#define DTAPI_TXSTUFF_MODE_ON       1

// Transmit polarity
#define DTAPI_TXPOL_NORMAL          0
#define DTAPI_TXPOL_INVERTED        1

// Upconverter RF modes
#define DTAPI_UPCONV_MODE           0            // Selects NORMAL/MUTE/CW/CWI/CWQ
#define DTAPI_UPCONV_MODE_MSK       0xF          // Mask for mode field
#define DTAPI_UPCONV_NORMAL         0
#define DTAPI_UPCONV_MUTE           1
#define DTAPI_UPCONV_CW             2
#define DTAPI_UPCONV_CWI            3
#define DTAPI_UPCONV_CWQ            4
#define DTAPI_UPCONV_SPECINV        0x100        // Can be OR-ed with other RF modes

// USB speed modes
#define DTAPI_USB_FULL_SPEED        0
#define DTAPI_USB_HIGH_SPEED        1
#define DTAPI_USB_SUPER_SPEED       2

// PCIe standards
#define DTAPI_PCIE_GEN_UNKNOWN      0       // PCIe Gen is unknown
#define DTAPI_PCIE_GEN1             1       // PCIe Gen 1 (2.5Gbps per link)
#define DTAPI_PCIE_GEN2             2       // PCIe Gen 2 (5.0Gbps per link)
#define DTAPI_PCIE_GEN3             3       // PCIe Gen 3 (8.0Gbps per link)

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Modulation Parameters -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-

// Modulation types
#define DTAPI_MOD_DVBS_QPSK         0            // Native DVB-S on DTA-107
#define DTAPI_MOD_DVBS_BPSK         1
#define DTAPI_MOD_QAM4              3
#define DTAPI_MOD_QAM16             4
#define DTAPI_MOD_QAM32             5
#define DTAPI_MOD_QAM64             6
#define DTAPI_MOD_QAM128            7
#define DTAPI_MOD_QAM256            8
#define DTAPI_MOD_DVBT              9
#define DTAPI_MOD_ATSC              10
#define DTAPI_MOD_DVBT2             11
#define DTAPI_MOD_ISDBT             12
#define DTAPI_MOD_ISDBS             13
#define DTAPI_MOD_IQDIRECT          15
#define DTAPI_MOD_IQ_2131           16           // DTA-2131 specific (de)modulation
#define DTAPI_MOD_DVBS2_QPSK        32
#define DTAPI_MOD_DVBS2_8PSK        33
#define DTAPI_MOD_DVBS2_16APSK      34
#define DTAPI_MOD_DVBS2_32APSK      35
#define DTAPI_MOD_DVBS2_L3          36     
#define DTAPI_MOD_DVBS2             37
#define DTAPI_MOD_DMBTH             48
#define DTAPI_MOD_ADTBT             49
#define DTAPI_MOD_CMMB              50
#define DTAPI_MOD_T2MI              51
#define DTAPI_MOD_DVBC2             52   
#define DTAPI_MOD_DAB               53   
#define DTAPI_MOD_QAM_AUTO          54   
#define DTAPI_MOD_ATSC_MH           55
#define DTAPI_MOD_ISDBTMM           56
// Modulation types DVB-S2X specific
#define DTAPI_MOD_S2X_QPSK_VLSNR    57           // DVB-S2X, QPSK, very low SNR
#define DTAPI_MOD_S2X_BPSK_VLSNR    58           // DVB-S2X, BPSK, very low SNR
#define DTAPI_MOD_S2X_BPSK_S_VLSNR  59           // DVB-S2X, BPSK-S, very low SNR
#define DTAPI_MOD_S2X_8APSK_L       60           // DVB-S2X, 8APSK-L
#define DTAPI_MOD_S2X_16APSK_L      61           // DVB-S2X, 16APSK-L
#define DTAPI_MOD_S2X_32APSK_L      62           // DVB-S2X, 32APSK-L
#define DTAPI_MOD_S2X_64APSK        63           // DVB-S2X, 64APSK
#define DTAPI_MOD_S2X_64APSK_L      64           // DVB-S2X, 64APSK-L
#define DTAPI_MOD_S2X_128APSK       65           // DVB-S2X, 128APSK
#define DTAPI_MOD_S2X_256APSK       66           // DVB-S2X, 256APSK-L
#define DTAPI_MOD_S2X_256APSK_L     67           // DVB-S2X, 256APSK
#define DTAPI_MOD_DVBS2X_L3         68           // L3 modulation with S2X support
#define DTAPI_MOD_TYPE_AUTO         -1           // Auto detect modulation type
#define DTAPI_MOD_TYPE_UNK          -1           // Unknown modulation type

// Modulation parameters - Common - ParXtra2
#define DTAPI_MOD_SYMRATE_AUTO      -1           // Auto detect symbol rate
#define DTAPI_MOD_SYMRATE_UNK       -1           // Symbol rate if unknown

// Modulation parameters - ATSC - ParXtra0
#define DTAPI_MOD_ATSC_VSB8         0x00000000   // 8-VSB, 10.762MBd, 19.392Mbps
#define DTAPI_MOD_ATSC_VSB16        0x00000001   // 16-VSB, 10.762MBd, 38.785Mbps
#define DTAPI_MOD_ATSC_VSB_AUTO     0x00000003   // Auto detect constellation
#define DTAPI_MOD_ATSC_VSB_UNK      0x00000003   // Unknown constellation
#define DTAPI_MOD_ATSC_VSB_MSK      0x00000003   // Constellation mask

// Modulation parameters - DTMB - Bandwidth
#define DTAPI_MOD_DTMB_5MHZ         0x00000001
#define DTAPI_MOD_DTMB_6MHZ         0x00000002
#define DTAPI_MOD_DTMB_7MHZ         0x00000003
#define DTAPI_MOD_DTMB_8MHZ         0x00000004
#define DTAPI_MOD_DTMB_BW_AUTO      0x0000000F   // Auto detect
#define DTAPI_MOD_DTMB_BW_UNK       0x0000000F   // Unknown
#define DTAPI_MOD_DTMB_BW_MSK       0x0000000F

// Modulation parameters - DTMB - Code rate
#define DTAPI_MOD_DTMB_0_4          0x00000100   // 0.4
#define DTAPI_MOD_DTMB_0_6          0x00000200   // 0.6
#define DTAPI_MOD_DTMB_0_8          0x00000300   // 0.8
#define DTAPI_MOD_DTMB_RATE_AUTO    0x00000F00   // Auto detect
#define DTAPI_MOD_DTMB_RATE_UNK     0x00000F00   // Unknown
#define DTAPI_MOD_DTMB_RATE_MSK     0x00000F00   // Mask

// Modulation parameters - DTMB - Constellation
#define DTAPI_MOD_DTMB_QAM4NR       0x00001000   // 4-QAM-NR
#define DTAPI_MOD_DTMB_QAM4         0x00002000   // 4-QAM
#define DTAPI_MOD_DTMB_QAM16        0x00003000   // 16-QAM
#define DTAPI_MOD_DTMB_QAM32        0x00004000   // 32-QAM
#define DTAPI_MOD_DTMB_QAM64        0x00005000   // 64-QAM
#define DTAPI_MOD_DTMB_CO_AUTO      0x0000F000   // Auto detect
#define DTAPI_MOD_DTMB_CO_UNK       0x0000F000   // Unknown
#define DTAPI_MOD_DTMB_CO_MSK       0x0000F000   // Mask

// Modulation parameters - DTMB - Frame header mode
#define DTAPI_MOD_DTMB_PN420        0x00010000   // PN420
#define DTAPI_MOD_DTMB_PN595        0x00020000   // PN595
#define DTAPI_MOD_DTMB_PN945        0x00030000   // PN945
#define DTAPI_MOD_DTMB_PN_AUTO      0x000F0000   // Auto detect
#define DTAPI_MOD_DTMB_PN_UNK       0x000F0000   // Unknown
#define DTAPI_MOD_DTMB_PN_MSK       0x000F0000   // Mask

// Modulation parameters - DTMB - Interleaver mode
#define DTAPI_MOD_DTMB_IL_1         0x00100000   // Interleaver mode 1: B=54, M=240
#define DTAPI_MOD_DTMB_IL_2         0x00200000   // Interleaver mode 2: B=54, M=720
#define DTAPI_MOD_DTMB_IL_AUTO      0x00F00000   // Auto detect
#define DTAPI_MOD_DTMB_IL_UNK       0x00F00000   // Unknown
#define DTAPI_MOD_DTMB_IL_MSK       0x00F00000   // Mask

// Modulation parameters - DTMB - pilots
#define DTAPI_MOD_DTMB_NO_PILOTS    0x01000000   // No pilots
#define DTAPI_MOD_DTMB_PILOTS       0x02000000   // Pilots, C=1 only
#define DTAPI_MOD_DTMB_PIL_AUTO     0x0F000000   // Auto detect
#define DTAPI_MOD_DTMB_PIL_UNK      0x0F000000   // Unknown
#define DTAPI_MOD_DTMB_PIL_MSK      0x0F000000   // Mask

// Modulation parameters - DTMB - Use frame numbering
#define DTAPI_MOD_DTMB_NO_FRM_NO    0x10000000   // No frame numbering
#define DTAPI_MOD_DTMB_USE_FRM_NO   0x20000000   // Use frame numbers
#define DTAPI_MOD_DTMB_UFRM_AUTO    0xF0000000   // Auto detect
#define DTAPI_MOD_DTMB_UFRM_UNK     0xF0000000   // Unknown
#define DTAPI_MOD_DTMB_UFRM_MSK     0xF0000000   // Mask

// Modulation parameters - DVB-S, DVB-S2
#define DTAPI_MOD_1_2               0x0          // Code rate 1/2
#define DTAPI_MOD_2_3               0x1          // Code rate 2/3
#define DTAPI_MOD_3_4               0x2          // Code rate 3/4
#define DTAPI_MOD_4_5               0x3          // Code rate 4/5
#define DTAPI_MOD_5_6               0x4          // Code rate 5/6
#define DTAPI_MOD_6_7               0x5          // Code rate 6/7
#define DTAPI_MOD_7_8               0x6          // Code rate 7/8
#define DTAPI_MOD_1_4               0x7          // Code rate 1/4
#define DTAPI_MOD_1_3               0x8          // Code rate 1/3
#define DTAPI_MOD_2_5               0x9          // Code rate 2/5
#define DTAPI_MOD_3_5               0xA          // Code rate 3/5
#define DTAPI_MOD_8_9               0xB          // Code rate 8/9
#define DTAPI_MOD_9_10              0xC          // Code rate 9/10
#define DTAPI_MOD_CR_AUTO           0xF          // Auto detect code rate
#define DTAPI_MOD_CR_UNK            0xF          // Unknown code rate
//Coderates DVB-S2X specific
#define DTAPI_MOD_1_5               0x10         // Code rate 1/5
#define DTAPI_MOD_2_9               0x11         // Code rate 2/9
#define DTAPI_MOD_11_45             0x12         // Code rate 11/45
#define DTAPI_MOD_4_15              0x13         // Code rate 4/15
#define DTAPI_MOD_13_45             0x14         // Code rate 13/45
#define DTAPI_MOD_14_45             0x15         // Code rate 14/45
#define DTAPI_MOD_9_20              0x16         // Code rate 9/20
#define DTAPI_MOD_7_15              0x17         // Code rate 7/15
#define DTAPI_MOD_8_15              0x18         // Code rate 8/15
#define DTAPI_MOD_11_20             0x19         // Code rate 11/20
#define DTAPI_MOD_5_9               0x1A         // Code rate 5/9
#define DTAPI_MOD_26_45             0x1B         // Code rate 26/45
#define DTAPI_MOD_28_45             0x1C         // Code rate 28/45
#define DTAPI_MOD_23_36             0x1D         // Code rate 23/36
#define DTAPI_MOD_29_45             0x1E         // Code rate 29/45
#define DTAPI_MOD_31_45             0x1F         // Code rate 31/45
#define DTAPI_MOD_25_36             0x20         // Code rate 25/36
#define DTAPI_MOD_32_45             0x21         // Code rate 32/45
#define DTAPI_MOD_13_18             0x22         // Code rate 13/18
#define DTAPI_MOD_11_15             0x23         // Code rate 11/15
#define DTAPI_MOD_7_9               0x24         // Code rate 7/9
#define DTAPI_MOD_77_90             0x25         // Code rate 77/90

// Modulation parameters - DVB-S, DVB-S2 - ParXtra1
#define DTAPI_MOD_S_S2_SPECNONINV   0x00         // No spectrum inversion detected
#define DTAPI_MOD_S_S2_SPECINV      0x10         // Spectrum inversion detected
#define DTAPI_MOD_S_S2_SPECINV_AUTO 0x30         // Auto detect spectral inversion
#define DTAPI_MOD_S_S2_SPECINV_UNK  0x30         // Spectral inversion is unknown
#define DTAPI_MOD_S_S2_SPECINV_MSK  0x30         // Mask for spectrum inversion field

// Modulation parameters - DVB-S2 - ParXtra1 - Pilots
#define DTAPI_MOD_S2_NOPILOTS       0x00         // Pilots disabled
#define DTAPI_MOD_S2_PILOTS         0x01         // Pilots enabled
#define DTAPI_MOD_S2_PILOTS_AUTO    0x03         // Auto detect pilots
#define DTAPI_MOD_S2_PILOTS_UNK     0x03         // State of pilots unknown
#define DTAPI_MOD_S2_PILOTS_MSK     0x03         // Mask for pilots field

// Modulation parameters - DVB-S2 - ParXtra1 - FEC frame length
#define DTAPI_MOD_S2_LONGFRM        0x00         // Long FECFRAME
#define DTAPI_MOD_S2_MEDIUMFRM      0x04         // Medium FECFRAME
#define DTAPI_MOD_S2_SHORTFRM       0x08         // Short FECFRAME
#define DTAPI_MOD_S2_FRM_AUTO       0x0C         // Auto detect frame size
#define DTAPI_MOD_S2_FRM_UNK        0x0C         // Frame size unknown
#define DTAPI_MOD_S2_FRM_MSK        0x0C         // Mask for FECFRAME field

// Modulation parameters - DVB-S2(X) - ParXtra1 - Constellation amplitude for 16-, 32-APSK
#define DTAPI_MOD_S2_CONST_AUTO     0x00         // Default constellation amplitude
#define DTAPI_MOD_S2_CONST_E_1      0x40         // E=1; Average symbol energy is constant
#define DTAPI_MOD_S2_CONST_R_1      0x80         // R=1; Radius of outer ring is constant
#define DTAPI_MOD_S2_CONST_MSK      0xC0         // Mask for constellation shape

// Modulation parameters - DVB-T - Bandwidth
#define DTAPI_MOD_DVBT_5MHZ         0x00000001
#define DTAPI_MOD_DVBT_6MHZ         0x00000002
#define DTAPI_MOD_DVBT_7MHZ         0x00000003
#define DTAPI_MOD_DVBT_8MHZ         0x00000004
#define DTAPI_MOD_DVBT_BW_UNK       0x0000000F   // Unknown bandwidth
#define DTAPI_MOD_DVBT_BW_MSK       0x0000000F

// Modulation parameters - DVB-T - Constellation
#define DTAPI_MOD_DVBT_QPSK         0x00000010
#define DTAPI_MOD_DVBT_QAM16        0x00000020
#define DTAPI_MOD_DVBT_QAM64        0x00000030
#define DTAPI_MOD_DVBT_CO_AUTO      0x000000F0   // Auto detect constellation
#define DTAPI_MOD_DVBT_CO_UNK       0x000000F0   // Unknown constellation
#define DTAPI_MOD_DVBT_CO_MSK       0x000000F0

// Modulation parameters - DVB-T - Guard interval
#define DTAPI_MOD_DVBT_G_1_32       0x00000100
#define DTAPI_MOD_DVBT_G_1_16       0x00000200
#define DTAPI_MOD_DVBT_G_1_8        0x00000300
#define DTAPI_MOD_DVBT_G_1_4        0x00000400
#define DTAPI_MOD_DVBT_GU_AUTO      0x00000F00   // Auto detect guard interval
#define DTAPI_MOD_DVBT_GU_UNK       0x00000F00   // Unknown guard interval
#define DTAPI_MOD_DVBT_GU_MSK       0x00000F00

// DVB-T TPS information - DVB-T Hierarchical layer
#define DTAPI_MOD_DVBT_HARCHY_NONE  0x00000000
#define DTAPI_MOD_DVBT_HARCHY_A1    0x01000000
#define DTAPI_MOD_DVBT_HARCHY_A2    0x02000000
#define DTAPI_MOD_DVBT_HARCHY_A4    0x03000000
#define DTAPI_MOD_DVBT_HARCHY_MSK   0x0F000000
// Modulation parameters - DVB-T - Interleaver mode
#define DTAPI_MOD_DVBT_INDEPTH      0x00001000
#define DTAPI_MOD_DVBT_NATIVE       0x00002000
#define DTAPI_MOD_DVBT_IL_AUTO      0x0000F000   // Auto detect interleaver depth
#define DTAPI_MOD_DVBT_IL_UNK       0x0000F000   // Unknown interleaver depth
#define DTAPI_MOD_DVBT_IL_MSK       0x0000F000

// Modulation parameters - DVB-T - FFT size
#define DTAPI_MOD_DVBT_2K           0x00010000
#define DTAPI_MOD_DVBT_4K           0x00020000
#define DTAPI_MOD_DVBT_8K           0x00030000
#define DTAPI_MOD_DVBT_MD_AUTO      0x000F0000   // Auto detect mode
#define DTAPI_MOD_DVBT_MD_UNK       0x000F0000   // Unknown mode
#define DTAPI_MOD_DVBT_MD_MSK       0x000F0000

// Modulation parameters - DVB-T - s48
#define DTAPI_MOD_DVBT_S48_OFF      0x00000000
#define DTAPI_MOD_DVBT_S48          0x00100000
#define DTAPI_MOD_DVBT_S48_MSK      0x00100000

// Modulation parameters - DVB-T - s49
#define DTAPI_MOD_DVBT_S49_OFF      0x00000000
#define DTAPI_MOD_DVBT_S49          0x00200000
#define DTAPI_MOD_DVBT_S49_MSK      0x00200000

// Modulation parameters - DVB-T - s48s49
#define DTAPI_MOD_DVBT_ENA4849      0x00000000
#define DTAPI_MOD_DVBT_DIS4849      0x00400000
#define DTAPI_MOD_DVBT_4849_MSK     0x00400000

// Modulation parameters - IQ - ParXtra0
#define DTAPI_MOD_INTERPOL_RAW      0            // Raw mode, no interpolation
#define DTAPI_MOD_INTERPOL_OFDM     1            // Use OFDM interpolation
#define DTAPI_MOD_INTERPOL_QAM      2            // Use QAM interpolation

// Modulation parameters - IQ - ParXtra2 Packing
#define DTAPI_MOD_IQPCK_AUTO        0x00000000   // Auto IQ-sample packin
#define DTAPI_MOD_IQPCK_NONE        0x00000001   // No IQ-sample packing
#define DTAPI_MOD_IQPCK_12B         0x00000002   // IQ-samples packed in 12-bit
#define DTAPI_MOD_IQPCK_10B         0x00000003   // IQ-samples packed in 10-bit
#define DTAPI_MOD_IQPCK_UNK         0x000000FF   // Unknown (= use auto)
#define DTAPI_MOD_IQPCK_MSK         0x000000FF

// Modulation parameters - Roll-off factor - ParXtra1 (DVB-S2), ParXtra2 (IQ)
#define DTAPI_MOD_ROLLOFF_AUTO      0x00000000   // Default roll-off factor
#define DTAPI_MOD_ROLLOFF_NONE      0x00000100   // No roll-off
#define DTAPI_MOD_ROLLOFF_5         0x00000200   // 5% roll-off for DVB-S2X
#define DTAPI_MOD_ROLLOFF_10        0x00000300   // 10% roll-off for DVB-S2X
#define DTAPI_MOD_ROLLOFF_15        0x00000400   // 15% roll-off for DVB-S2X
#define DTAPI_MOD_ROLLOFF_20        0x00000500   // 20% roll-off for DVB-S2
#define DTAPI_MOD_ROLLOFF_25        0x00000600   // 25% roll-off for DVB-S2
#define DTAPI_MOD_ROLLOFF_35        0x00000700   // 35% roll-off for DVB-S/S2
#define DTAPI_MOD_ROLLOFF_UNK       0x0000FF00   // Unknown (= use default)
#define DTAPI_MOD_ROLLOFF_MSK       0x0000FF00

// Modulation parameters - DVB-T2-MI - ParXtra0 used for T2-MI bitrate

// Modulation parameters - DVB-T2-MI - ParXtra1
#define DTAPI_MOD_T2MI_PID1_MSK     0x1FFF
#define DTAPI_MOD_T2MI_PID1_SHFT    0
#define DTAPI_MOD_T2MI_PID2_MSK     0x1FFF0000
#define DTAPI_MOD_T2MI_PID2_SHFT    16
#define DTAPI_MOD_T2MI_MULT_DIS     0x00000000   // Single Profile
#define DTAPI_MOD_T2MI_MULT_ENA     0x20000000   // Multi Profile
#define DTAPI_MOD_T2MI_MULT_MSK     0x20000000   // Multi Profile mask

// Modulation parameters - QAM - ParXtra0 - J.83 Annex
#define DTAPI_MOD_J83_MSK           0x000F
#define DTAPI_MOD_J83_UNK           0x000F       // Unknown annex
#define DTAPI_MOD_J83_AUTO          0x000F       // Auto detect annex
#define DTAPI_MOD_J83_A             0x0002       // J.83 annex A (DVB-C)
#define DTAPI_MOD_J83_B             0x0003       // J.83 annex B (\93American QAM\94)
#define DTAPI_MOD_J83_C             0x0001       // J.83 annex C (\93Japanese QAM\94)

// Modulation parameters - QAM - ParXtra1 - QAM-B interleaver mode
#define DTAPI_MOD_QAMB_I128_J1D     0x1
#define DTAPI_MOD_QAMB_I64_J2       0x3
#define DTAPI_MOD_QAMB_I32_J4       0x5
#define DTAPI_MOD_QAMB_I16_J8       0x7
#define DTAPI_MOD_QAMB_I8_J16       0x9
#define DTAPI_MOD_QAMB_I128_J1      0x0
#define DTAPI_MOD_QAMB_I128_J2      0x2
#define DTAPI_MOD_QAMB_I128_J3      0x4
#define DTAPI_MOD_QAMB_I128_J4      0x6
#define DTAPI_MOD_QAMB_I128_J5      0x8
#define DTAPI_MOD_QAMB_I128_J6      0xA
#define DTAPI_MOD_QAMB_I128_J7      0xC
#define DTAPI_MOD_QAMB_I128_J8      0xE
#define DTAPI_MOD_QAMB_IL_UNK       0xF          // Unknown interleaver mode
#define DTAPI_MOD_QAMB_IL_AUTO      0xF          // Auto detect interleaver mode
#define DTAPI_MOD_QAMB_IL_MSK       0xF

//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ SDI +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

// DtSdi - Table-of-content entry types
#define DTAPI_SDI_TOC_ENTRY_UNKNOWN 0
#define DTAPI_SDI_TOC_ENTRY_ACTVID  1
#define DTAPI_SDI_TOC_ENTRY_HANC    2
#define DTAPI_SDI_TOC_ENTRY_VANC    3

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtSdiTocEntry -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
class DtSdiTocEntry
{
    friend class DtSdiUtility;

public:
    inline int  AncDataBlockNum() const 
    { 
        if (AncType() != 1)   return -1;
        else                  return m_SdidOrDbn;
    }
    inline int  AncDataId() const { return m_Did; }
    inline int  AncNumUserWords() const { return m_NumUserWords; }
    inline int  AncSecDataId() const
    { 
        if (AncType() != 2)   return -1;
        else                  return m_SdidOrDbn;
    }
    inline int  AncType() const { return m_AncType; }

    inline int  Field() const { return m_Field; }
    inline int  Line() const { return m_Line; }
    inline int  NumSymbols() const { return m_NumSymbols; }
    inline int  StartOffset() const { return m_StartOffset; }
    inline int  TocType() const { return m_TocType; }

    // Encapsulated data
protected:
    int  m_TocType;                 // Type of TOC entry
    int  m_Line;                    // Line number where data is located
    int  m_Field;                   // Field in where data is located
    int  m_StartOffset;             // Symbol offset relative to start of line, first data
    int  m_NumSymbols;              // Number of symbols

    // Following members are only valid if TOC type is DTAPI_SDI_TOC_ENTRY_HANC or 
    // DTAPI_SDI_TOC_ENTRY_VANC
    int  m_AncType;                 // Ancillery data packet type (DTAPI_SDI_ANC_TYPE1 or
                                    // DTAPI_SDI_ANC_TYPE2)
    int  m_Did;                     // Ancillary packet data ID
    int  m_SdidOrDbn;               // Ancillary packet data block number (type 1 packet)
                                    // or secondary data ID (type 2 packet)
    int  m_NumUserWords;            // Number of ancillary data packet user words

    // Constructor, destructor
public:
    DtSdiTocEntry() : m_TocType(DTAPI_SDI_TOC_ENTRY_UNKNOWN), m_Line(0), m_Field(0),
                      m_StartOffset(0), m_NumSymbols(0) {}
    virtual ~DtSdiTocEntry() {};
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtSdi -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// The DtSdi class provides helper functions for processing SDI data
// 
class DtSdi
{
    friend class DtSdiUtility;

public:
    DtSdi();
    virtual ~DtSdi();

public:
    DTAPI_RESULT  ConvertFrame(unsigned int* pInFrame, int& InFrameSize,
       int InFrameFormat, unsigned int* pOutFrame, int& OutFrameSize, int OutFrameFormat);
    DTAPI_RESULT  CreateBlackFrame(unsigned int* pFrame, int& FrameSize, int FrameFormat);
    DTAPI_RESULT  GetActiveVideo(const DtSdiTocEntry& TocEntry,
                                                 unsigned short* pVideo, int& NumSamples);
    DTAPI_RESULT  GetActiveVideo(unsigned short* pVideo, int& NumSamples,
                                                                int Field, int Stride=-1);
    DTAPI_RESULT  GetAncillaryData(const DtSdiTocEntry& TocEntry,
                                                  unsigned short* pData, int& NumSamples);
    DTAPI_RESULT  GetAudio(int AudioGroup, int& Channel,
                                                 unsigned short* pAudio, int& NumSamples);
    DTAPI_RESULT  GetTableOfContents(const DtSdiTocEntry** ppToc, int& NumTocEntries);
    DTAPI_RESULT  ParseFrame(const unsigned int* pFrame, int FrameSize, int FrameFormat,
                         int ParseFlags, const DtSdiTocEntry** ppToc, int& NumTocEntries);    

protected:
    DtSdiUtility*  m_pSdiUtil;      // Internal utility class
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtSdi constants -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.

// Ancillary data packet types
#define DTAPI_SDI_ANC_TYPE1         1            // Type 1 packet
#define DTAPI_SDI_ANC_TYPE2         2            // Type 2 packet

// Parse flags
#define DTAPI_SDI_PARSE_ACTVID      0x0001       // Parse active video
#define DTAPI_SDI_PARSE_HBLANK      0x0002       // Parse horizontal blanking
#define DTAPI_SDI_PARSE_VBLANK      0x0004       // Parse vertical blanking

#define DTAPI_SDI_PARSE_BLANK       (DTAPI_SDI_PARSE_HBLANK | DTAPI_SDI_PARSE_VBLANK)
#define DTAPI_SDI_PARSE_ALL         (DTAPI_SDI_PARSE_ACTVID | DTAPI_SDI_PARSE_BLANK)

// Field flags
#define DTAPI_SDI_FIELD1            1
#define DTAPI_SDI_FIELD2            2

// Audio groups
#define DTAPI_SDI_AUDIO_GROUP1      0x2FF
#define DTAPI_SDI_AUDIO_GROUP2      0x1FD
#define DTAPI_SDI_AUDIO_GROUP3      0x1FB
#define DTAPI_SDI_AUDIO_GROUP4      0x2F9

#define DTAPI_SDI_AUDIO_CHAN1       0x01
#define DTAPI_SDI_AUDIO_CHAN2       0x02
#define DTAPI_SDI_AUDIO_CHAN3       0x04
#define DTAPI_SDI_AUDIO_CHAN4       0x08

#define DTAPI_SDI_AUDIO_CH_PAIR1    (DTAPI_SDI_AUDIO_CHAN1 | DTAPI_SDI_AUDIO_CHAN2)
#define DTAPI_SDI_AUDIO_CH_PAIR2    (DTAPI_SDI_AUDIO_CHAN3 | DTAPI_SDI_AUDIO_CHAN4)
#define DTAPI_SDI_AUDIO_CH_MASK     (DTAPI_SDI_AUDIO_CH_PAIR1 | DTAPI_SDI_AUDIO_CH_PAIR2)

// Conversions format
#define DTAPI_SDI_FULL              0x001
#define DTAPI_SDI_ACTVID            0x002
#define DTAPI_SDI_HUFFMAN           0x004
#define DTAPI_SDI_625               0x008
#define DTAPI_SDI_525               0x010
#define DTAPI_SDI_8B                0x020
#define DTAPI_SDI_10B               0x040
#define DTAPI_SDI_16B               0x080
#define DTAPI_SDI_10B_NBO           0x100   // 10-bit packed in network-byte-order

#define DTAPI_SDI_BIT_MASK          0x1E0

// Video standards
#define DTAPI_VIDSTD_UNKNOWN        -1
#define DTAPI_VIDSTD_525I59_94      DTAPI_IOCONFIG_525I59_94
#define DTAPI_VIDSTD_625I50         DTAPI_IOCONFIG_625I50
#define DTAPI_VIDSTD_720P23_98      DTAPI_IOCONFIG_720P23_98
#define DTAPI_VIDSTD_720P24         DTAPI_IOCONFIG_720P24
#define DTAPI_VIDSTD_720P25         DTAPI_IOCONFIG_720P25
#define DTAPI_VIDSTD_720P29_97      DTAPI_IOCONFIG_720P29_97
#define DTAPI_VIDSTD_720P30         DTAPI_IOCONFIG_720P30
#define DTAPI_VIDSTD_720P50         DTAPI_IOCONFIG_720P50
#define DTAPI_VIDSTD_720P59_94      DTAPI_IOCONFIG_720P59_94
#define DTAPI_VIDSTD_720P60         DTAPI_IOCONFIG_720P60
#define DTAPI_VIDSTD_1080P23_98     DTAPI_IOCONFIG_1080P23_98
#define DTAPI_VIDSTD_1080P24        DTAPI_IOCONFIG_1080P24
#define DTAPI_VIDSTD_1080P25        DTAPI_IOCONFIG_1080P25
#define DTAPI_VIDSTD_1080P30        DTAPI_IOCONFIG_1080P30
#define DTAPI_VIDSTD_1080P29_97     DTAPI_IOCONFIG_1080P29_97
#define DTAPI_VIDSTD_1080I50        DTAPI_IOCONFIG_1080I50
#define DTAPI_VIDSTD_1080I59_94     DTAPI_IOCONFIG_1080I59_94
#define DTAPI_VIDSTD_1080I60        DTAPI_IOCONFIG_1080I60
#define DTAPI_VIDSTD_1080P50        DTAPI_IOCONFIG_1080P50
#define DTAPI_VIDSTD_1080P59_94     DTAPI_IOCONFIG_1080P59_94
#define DTAPI_VIDSTD_1080P60        DTAPI_IOCONFIG_1080P60

// Audio standard
#define DTAPI_SDI_AUDIO_SMPTE272A   1            // SMPTE-272 Level A, 48kHz, 20-bit audio

// Audio formats
#define DTAPI_SDI_AUDIO_PCM16       0
#define DTAPI_SDI_AUDIO_PCM24       1
#define DTAPI_SDI_AUDIO_PCM32       2

// HANC/VANC/Video selection (can be OR-ed)
//#define DTAPI_SDI_ACTVID               0x01
#define DTAPI_SDI_HANC              0x02
#define DTAPI_SDI_VANC              0x04
#define DTAPI_SDI_ANC_MASK          (DTAPI_SDI_HANC | DTAPI_SDI_VANC)
    
// Chrominace/luminance stream selection (can be OR-ed)
#define DTAPI_SDI_CHROM             0x01
#define DTAPI_SDI_LUM               0x02
#define DTAPI_SDI_STREAM_MASK       (DTAPI_SDI_CHROM | DTAPI_SDI_LUM)

// Anc-data operation mode
#define DTAPI_ANC_MARK              0x0001
#define DTAPI_ANC_DELETE            0x0002

// Scalling factor
#define DTAPI_SCALING_OFF           1
#define DTAPI_SCALING_1_4           2
#define DTAPI_SCALING_1_16          3

// Symbol filter mode
#define DTAPI_SYMFLT_ALL            0       // YCbCr sample (CbYCrY order)
#define DTAPI_SYMFLT_LUM            1       // Luminance only (Y) 
#define DTAPI_SYMFLT_CHROM          2       // Chrominance only (CbCr)
#define DTAPI_SYMFLT_SWAP           3       // Swap order of lum and chrom (i.e. YCbYCr)
#define DTAPI_SYMFLT_RGB            4       // Convert to/from RGB

// Ancillary filter mode
#define DTAPI_ANCFLT_OFF            0
#define DTAPI_ANCFLT_HANC_ALL       1
#define DTAPI_ANCFLT_HANC_MIN       2
#define DTAPI_ANCFLT_VANC_ALL       3
#define DTAPI_ANCFLT_VANC_MIN       4

// Receive mode hints for USB3 devices
#define DTAPI_RXMODE_FRAMEBUFFER      0x10000
#define DTAPI_RXMODE_ANC              (DTAPI_RXMODE_FRAMEBUFFER | 1)
#define DTAPI_RXMODE_RAW              (DTAPI_RXMODE_FRAMEBUFFER | 2)
#define DTAPI_RXMODE_FULL             (DTAPI_RXMODE_FRAMEBUFFER | 3)
#define DTAPI_RXMODE_FULL8            (DTAPI_RXMODE_FRAMEBUFFER | 4)
#define DTAPI_RXMODE_FULL8_SCALED4    (DTAPI_RXMODE_FRAMEBUFFER | 5)
#define DTAPI_RXMODE_FULL8_SCALED16   (DTAPI_RXMODE_FRAMEBUFFER | 6)
#define DTAPI_RXMODE_VIDEO            (DTAPI_RXMODE_FRAMEBUFFER | 7)
#define DTAPI_RXMODE_VIDEO8           (DTAPI_RXMODE_FRAMEBUFFER | 8)
#define DTAPI_RXMODE_VIDEO8_SCALED4   (DTAPI_RXMODE_FRAMEBUFFER | 9)
#define DTAPI_RXMODE_VIDEO8_SCALED16  (DTAPI_RXMODE_FRAMEBUFFER | 10)
#define DTAPI_RXMODE_RAW8             (DTAPI_RXMODE_FRAMEBUFFER | 15)
#define DTAPI_RXMODE_FRMBUF_MASK      0x0F

//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ HD-SDI CLASSES +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- AncPacket -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
class AncPacket
{
public:
    AncPacket();
    AncPacket(const AncPacket& s);
    virtual ~AncPacket();

public:
    int  m_Did;                     // Data identifier
    int  m_SdidOrDbn;               // Secondary data identifier / Data block number
    int  m_Dc;                      // Data count
    int  m_Cs;                      // Check sum
    unsigned short*  m_pUdw;        // User data words

    // Operations
public:
    void  Create(unsigned short*  pUserWords, int  NumWords);
    void  Create(int  NumWords=256);
    void  Destroy();
    int  Type() const  { return (m_Did & 0x80)==0 ? 2 : 1; }
    int Size() const { return m_Size; }

    void  operator = (const AncPacket& s);

private:
    int  m_Size;                    // Size of user data buffer (in # of words)
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameBufTrPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
class DtFrameBufTrPars
{
public:
    enum ParsType { PT_VIDEO, PT_ANC, PT_RAW };

protected:
    DtFrameBufTrPars(ParsType  Type);
public:
    virtual ~DtFrameBufTrPars();

    // Operations
public:
    DTAPI_RESULT  SetCommon(__int64  Frame, unsigned char* pBuf,  int  BufSize,  
                                    int  DataFormat, int  StartLine=1,  int  NumLines=-1);

    ParsType GetType() const { return m_Type; } 

public:
    __int64  m_Frame;           // Frame to transfer
    unsigned char* m_pBuf;      // Transfer buffer
    int  m_BufSize;             // [in] size of buffer / [out] actual #bytes transferred
    int  m_StartLine;           // [in] 1st line to transfer / [out] actual first line
    int  m_NumLines;            // [in] #lines to transfer / [out] actual #lines
    int  m_DataFormat;          // Format of data (8-, 10-, 16-bit)   

private:
    ParsType  m_Type;           // Parameter type
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameBufTrParsVideo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
class DtFrameBufTrParsVideo : public DtFrameBufTrPars
{
public:
    DtFrameBufTrParsVideo(int  Field, int  Scaling=DTAPI_SCALING_OFF, int  Stride=-1,
                                                            int  SymFlt=DTAPI_SYMFLT_ALL);
    virtual ~DtFrameBufTrParsVideo();
    
public:
    int  m_Field;               // Field to transfer
    int  m_Scaling;             // Scaling mode
    int  m_Stride;              // -1 means no stride
    int  m_SymFlt;              // Symbol filter mode
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameBufTrParsAnc -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
class DtFrameBufTrParsAnc : public DtFrameBufTrPars
{
public:
    DtFrameBufTrParsAnc(int  HancVanc, int  AncFlt=DTAPI_ANCFLT_OFF);
    virtual ~DtFrameBufTrParsAnc();

public:
    int  m_HancVanc;            // HANC or VANC
    int  m_AncFlt;              // Anc filter mode
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameBufTrParsRaw -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
class DtFrameBufTrParsRaw : public DtFrameBufTrPars
{
public:
    DtFrameBufTrParsRaw(int  SymFlt=DTAPI_SYMFLT_ALL);
    virtual ~DtFrameBufTrParsRaw();

public:
    int  m_SymFlt;              // Symbol filter mode
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtFrameBuffer -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class to represent an frame buffer
//
class DtFrameBuffer
{
public:
    DtFrameBuffer();
    virtual ~DtFrameBuffer();
private:
    // No implementation is provided for the copy constructor
    DtFrameBuffer(const DtFrameBuffer&);

public:
    virtual DTAPI_RESULT  AncAddAudio(__int64 Frame, unsigned char* pBuf,
                                  int& BufSize, int Format, int Channels, int AudioGroup);
    virtual DTAPI_RESULT  AncAddAudioStatusWord(__int64 Frame, unsigned char  Status[24],
                                                          int  Channels, int  AudioGroup);
    virtual DTAPI_RESULT  AncAddPacket(__int64 Frame, AncPacket&  AncPacket,
                                                      int Line, int HancVanc, int Stream);
    virtual DTAPI_RESULT  AncClear(__int64 Frame, int HancVanc, int Stream);
    virtual DTAPI_RESULT  AncCommit(__int64 Frame);
    virtual DTAPI_RESULT  AncDelAudio(__int64 Frame, int AudioGroup, int Mode);
    virtual DTAPI_RESULT  AncDelPacket(__int64 Frame, int DID, int SDID, int StartLine,
                                        int NumLines, int HancVanc, int Stream, int Mode);
    virtual DTAPI_RESULT  AncGetAudio(__int64 Frame, unsigned char*  pBuf, 
                           int&  BufSize, int DataFormat, int&  Channels, int AudioGroup);
    virtual DTAPI_RESULT  AncGetPacket(__int64 Frame, int DID, int SDID, 
                                  AncPacket*, int&  NumPackets, int HancVanc, int Stream);
    virtual DTAPI_RESULT  AncReadRaw(__int64 Frame, unsigned char* pBuf,
                                int&  BufSize, int DataFormat, int StartLine,
                                int NumLines, int HancVanc, bool EnableAncFilter = false);
    virtual DTAPI_RESULT  AncReadRaw(DtFrameBufTrParsAnc&  TP);
    virtual DTAPI_RESULT  AncWriteRaw(__int64 Frame, unsigned char* pBuf, 
                                int&  BufSize, int Format, int StartLine, 
                                int NumLines, int HancVanc, bool EnableAncFilter = false);
    virtual DTAPI_RESULT  AncWriteRaw(DtFrameBufTrParsAnc&  TP);
    virtual DTAPI_RESULT  AttachToInput(DtDevice*, int Port);
    virtual DTAPI_RESULT  AttachToOutput(DtDevice*, int Port, int Delay);
    virtual DTAPI_RESULT  ClearFlags(int Latched);
    virtual DTAPI_RESULT  Detach();
    virtual DTAPI_RESULT  DetectIoStd(int& Value, int& SubValue);
    virtual DTAPI_RESULT  GetBufferInfo(DtBufferInfo&);
    virtual DTAPI_RESULT  GetCurFrame(__int64& CurFrame);
    virtual DTAPI_RESULT  GetFlags(int& Flags, int& Latched);
    virtual DTAPI_RESULT  GetFrameInfo(__int64 Frame, DtFrameInfo&);
    virtual DTAPI_RESULT  GetStatistics(int Count, DtStatistic* pStatistics);
    virtual DTAPI_RESULT  GetStatistic(int Type, int& Statistic);
    virtual DTAPI_RESULT  ReadSdiLines(__int64 Frame, unsigned char* pBuf, 
                              int& BufSize, int DataFormat, int StartLine, int& NumLines);
    virtual DTAPI_RESULT  ReadSdiLines(DtFrameBufTrParsRaw&  TP);
    virtual DTAPI_RESULT  ReadSdiLines(__int64 Frame, unsigned char* pBuf, 
                                                           int&  BufSize, int DataFormat);
    virtual DTAPI_RESULT  ReadVideo(__int64 Frame, unsigned char* pBuf,
                                           int& BufSize, int Field, int FullOrScaled,
                                           int DataFormat, int StartLine, int&  NumLines);
    virtual DTAPI_RESULT  ReadVideo(DtFrameBufTrParsVideo&  TP);
    virtual DTAPI_RESULT  SetRxMode(int  RxMode, __int64&  FirstFrame);
    virtual DTAPI_RESULT  Start(bool Start=true);
    virtual DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    virtual DTAPI_RESULT  WaitFrame(__int64& LastFrame);
    virtual DTAPI_RESULT  WaitFrame(__int64& FirstFrame, __int64& LastFrame);
    virtual DTAPI_RESULT  WriteSdiLines(__int64 Frame, unsigned char* pBuf, 
                                                            int& BufSize, int DataFormat);
    virtual DTAPI_RESULT  WriteSdiLines(__int64 Frame, unsigned char*  pBuf, 
                            int&  BufSize, int DataFormat, int StartLine, int&  NumLines);
    virtual DTAPI_RESULT  WriteSdiLines(DtFrameBufTrParsRaw&  TP);
    virtual DTAPI_RESULT  WriteVideo(__int64 Frame, unsigned char*  pBuf, int&  BufSize,
                                int Field, int DataFormat, int StartLine, int&  NumLines);
    virtual DTAPI_RESULT  WriteVideo(DtFrameBufTrParsVideo&  TP);

protected:
    FrameBufImpl*  m_pImpl;         // Implementation class
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtSdiMatrix -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
class DtSdiMatrix
{
public:
    DtSdiMatrix();
    virtual ~DtSdiMatrix();
private:
    // No implementation is provided for the copy constructor
    DtSdiMatrix(const DtSdiMatrix&);

public:
    virtual DTAPI_RESULT  Attach(DtDevice*  pDvc, int&  MaxNumRows);
    virtual DTAPI_RESULT  Detach();
    virtual DTAPI_RESULT  GetMatrixInfo(DtMatrixInfo&  Info);
    virtual DtFrameBuffer&  Row(int n);
    virtual DTAPI_RESULT  Start(bool  Start=true);
    virtual DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue = -1,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    virtual DTAPI_RESULT  WaitFrame(__int64&  LastFrame);
    virtual DTAPI_RESULT  WaitFrame(__int64&  FirstFrame, __int64&  LastFrame);
    
private:
    SdiMatrixImpl*  m_pImpl;        // Implementation class
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Global DTAPI Functions -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.

DTAPI_RESULT  DtapiCheckDeviceDriverVersion(void);
DTAPI_RESULT  DtapiCheckDeviceDriverVersion(int DeviceCategory);
DTAPI_RESULT  DtapiDeviceScan(int NumEntries, int& NumEntriesResult,
                                         DtDeviceDesc* DvcDescArr, bool InclDteDvcs=false,
                                         int  ScanOrder=DTAPI_SCANORDER_ORIG);
DTAPI_RESULT  DtapiDtaPlusDeviceScan(int NumEntries, int& NumEntriesResult,
                                                         DtDtaPlusDeviceDesc* DvcDescArr);
DTAPI_RESULT  DtapiDtDeviceDesc2String(DtDeviceDesc* pDvcDesc, int StringType, 
                                                         char* pString, int StringLength);
DTAPI_RESULT  DtapiDtDeviceDesc2String(DtDeviceDesc* pDvcDesc, int StringType, 
                                                      wchar_t* pString, int StringLength);
DTAPI_RESULT  DtapiDtHwFuncDesc2String(DtHwFuncDesc* pHwFunc, int StringType, 
                                                         char* pString, int StringLength);
DTAPI_RESULT  DtapiDtHwFuncDesc2String(DtHwFuncDesc* pHwFunc, int StringType, 
                                                      wchar_t* pString, int StringLength);
DTAPI_RESULT  DtapiGetDeviceDriverVersion(int, int&, int&, int&, int&);
DTAPI_RESULT  DtapiGetDtapiServiceVersion(int&, int&, int&, int&);
DTAPI_RESULT  DtapiGetVersion(int& Maj, int& Min, int& BugFix, int& Build);
DTAPI_RESULT  DtapiHwFuncScan(int NumEntries, int& NumEntriesResult,
                                           DtHwFuncDesc* pHwFuncs, bool InclDteDvcs=false,
                                           int  ScanOrder=DTAPI_SCANORDER_ORIG);
DTAPI_RESULT  DtapiPower2Voltage(int dBm, int& dBmV, bool Is50Ohm=false);
const char*  DtapiResult2Str(DTAPI_RESULT DtapiResult);
DTAPI_RESULT  DtapiVoltage2Power(int dBmV, int& dBm, bool Is50Ohm=false);

// Callbacks
DTAPI_RESULT  DtapiRegisterCallback(pDtEventCallback Callback, void* pContext,
                                                       int EventTypes, void** pId = NULL);
DTAPI_RESULT  DtapiUnregisterCallback(void* pId);

// IP address conversion
DTAPI_RESULT  DtapiInitDtIpParsFromIpString(DtIpPars& IpPars,
                                                  const char* pDstIp, const char* pSrcIp);
DTAPI_RESULT  DtapiInitDtIpParsFromIpString(DtIpPars& IpPars,
                                            const wchar_t* pDstIp, const wchar_t* pSrcIp);
DTAPI_RESULT  DtapiIpAddr2ByteArray(const char* pIpStr, unsigned char* pIpByte,
                                                                              int& Flags);
DTAPI_RESULT  DtapiIpAddr2ByteArray(const wchar_t* pIpStr, 
                                                      unsigned char* pIpByte, int& Flags);
DTAPI_RESULT  DtapiIpAddr2Str(char* pStr, int Len, unsigned char* pIpAddr);
DTAPI_RESULT  DtapiIpAddr2Str(wchar_t* pStr, int Len, unsigned char* pIpAddr);
DTAPI_RESULT  DtapiStr2IpAddr(unsigned char* pIpAddr, const char* pStr);
DTAPI_RESULT  DtapiStr2IpAddr(unsigned char* pIpAddr, const wchar_t* pStr);

// Legacy
#define DtapiInitDtTsIpParsFromIpString DtapiInitDtIpParsFromIpString

// Modulator functions
DTAPI_RESULT  DtapiModPars2Bandwidth(int& ModBandwidth, int& TotalBandwidth,
                                    int ModType, int ParXtra0, int ParXtra1, int ParXtra2,
                                    void* pXtraPars, int SymRate);
DTAPI_RESULT  DtapiModPars2SymRate(int& SymRate, int ModType, int ParXtra0,
                                                  int ParXtra1, int ParXtra2, int TsRate);
DTAPI_RESULT  DtapiModPars2SymRate(int& SymRate, int ModType, int ParXtra0,
                                 int ParXtra1, int ParXtra2, void* pXtraPars, int TsRate);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, int ModType, int ParXtra0,
                                            int ParXtra1, int ParXtra2, int SymRate = -1);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, DtDvbC2Pars&, int PlpIdx = 0);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, DtDvbS2Pars&, int PlpIdx = 0);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, DtDvbT2Pars&, int PlpIdx = 0);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, DtIsdbTmmPars&, int TsIdx = 0);
DTAPI_RESULT  DtapiModPars2TsRate(int& TsRate, int ModType, int ParXtra0,
                                int ParXtra1, int ParXtra2, void* pXtraPars, int SymRate);

// HD-SDI functions
DTAPI_RESULT  DtapiGetRequiredUsbBandwidth(int VidStd, int RxMode, long long&  Bandwidth);
DTAPI_RESULT  DtapiGetVidStdInfo(int VidStd, DtVidStdInfo& Info);
DTAPI_RESULT  DtapiIoStd2VidStd(int Value, int SubValue, int& VidStd);
DTAPI_RESULT  DtapiVidStd2IoStd(int VidStd, int& Value, int& SubValue);
const char*  DtapiVidStd2Str(int VidStd);

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- Return Codes -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// NOTE: ERROR CODES 0x1100-0x12FF ARE RESERVED FOR USE IN THE DTAPIplus
//
#define DTAPI_OK                    0
#define DTAPI_OK_FAILSAFE           1
#define DTAPI_E                     0x1000
#define DTAPI_E_ATTACHED            (DTAPI_E + 0)
#define DTAPI_E_BUF_TOO_SMALL       (DTAPI_E + 1)
#define DTAPI_E_DEV_DRIVER          (DTAPI_E + 2)
#define DTAPI_E_EEPROM_FULL         (DTAPI_E + 3)
#define DTAPI_E_EEPROM_READ         (DTAPI_E + 4)
#define DTAPI_E_EEPROM_WRITE        (DTAPI_E + 5)
#define DTAPI_E_EEPROM_FORMAT       (DTAPI_E + 6)
#define DTAPI_E_FIFO_FULL           (DTAPI_E + 7)
#define DTAPI_E_IN_USE              (DTAPI_E + 8)
#define DTAPI_E_INVALID_BUF         (DTAPI_E + 9)
#define DTAPI_E_INVALID_FLAGS       (DTAPI_E + 11)
#define DTAPI_E_INVALID_MODE        (DTAPI_E + 12)
#define DTAPI_E_INVALID_RATE        (DTAPI_E + 13)
#define DTAPI_E_INVALID_SIZE        (DTAPI_E + 14)
#define DTAPI_E_KEYWORD             (DTAPI_E + 15)
#define DTAPI_E_NO_DEVICE           (DTAPI_E + 16)
#define DTAPI_E_NO_LOOPBACK         (DTAPI_E + 17)
#define DTAPI_E_NO_SUCH_DEVICE      (DTAPI_E + 18)
#define DTAPI_E_NO_SUCH_OUTPUT      (DTAPI_E + 19)
#define DTAPI_E_NO_DT_OUTPUT        (DTAPI_E + 20)
#define DTAPI_E_NO_TS_OUTPUT        (DTAPI_E + 20)
#define DTAPI_E_NOT_ATTACHED        (DTAPI_E + 21)
#define DTAPI_E_NOT_FOUND           (DTAPI_E + 22)
#define DTAPI_E_NOT_SUPPORTED       (DTAPI_E + 23)
#define DTAPI_E_DEVICE              (DTAPI_E + 24)
#define DTAPI_E_TOO_LONG            (DTAPI_E + 25)
#define DTAPI_E_UNDERFLOW           (DTAPI_E + 26)
#define DTAPI_E_NO_SUCH_INPUT       (DTAPI_E + 27)
#define DTAPI_E_NO_DT_INPUT         (DTAPI_E + 28)
#define DTAPI_E_NO_TS_INPUT         (DTAPI_E + 28)
#define DTAPI_E_DRIVER_INCOMP       (DTAPI_E + 29)
#define DTAPI_E_INTERNAL            (DTAPI_E + 30)
#define DTAPI_E_OUT_OF_MEM          (DTAPI_E + 31)
#define DTAPI_E_INVALID_J83ANNEX    (DTAPI_E + 32)
#define DTAPI_E_IDLE                (DTAPI_E + 33)
#define DTAPI_E_INSUF_LOAD          (DTAPI_E + 34)
#define DTAPI_E_INVALID_BANDWIDTH   (DTAPI_E + 35)
#define DTAPI_E_INVALID_CONSTEL     (DTAPI_E + 36)
#define DTAPI_E_INVALID_GUARD       (DTAPI_E + 37)
#define DTAPI_E_INVALID_INTERLVNG   (DTAPI_E + 38)
#define DTAPI_E_INVALID_TRANSMODE   (DTAPI_E + 39)
#define DTAPI_E_INVALID_TSTYPE      (DTAPI_E + 40)
#define DTAPI_E_NO_IPPARS           (DTAPI_E + 41)
#define DTAPI_E_NO_TSRATE           (DTAPI_E + 42)
#define DTAPI_E_NOT_IDLE            (DTAPI_E + 43)
#define DTAPI_E_INVALID_ARG         (DTAPI_E + 44)
#define DTAPI_E_NW_DRIVER           (DTAPI_E + 45)
#define DTAPI_E_DST_MAC_ADDR        (DTAPI_E + 46)
#define DTAPI_E_NO_SUCH_PORT        (DTAPI_E + 47)
#define DTAPI_E_WINSOCK             (DTAPI_E + 48)
#define DTAPI_E_MULTICASTJOIN       (DTAPI_E + 49)
#define DTAPI_E_EMBEDDED            (DTAPI_E + 50)
#define DTAPI_E_LOCKED              (DTAPI_E + 51)
#define DTAPI_E_NO_VALID_CALDATA    (DTAPI_E + 52)
#define DTAPI_E_NO_LINK             (DTAPI_E + 53)
#define DTAPI_E_INVALID_HEADER      (DTAPI_E + 54)
#define DTAPI_E_INVALID_PARS        (DTAPI_E + 55)
#define DTAPI_E_NOT_SDI_MODE        (DTAPI_E + 56)
#define DTAPI_E_INCOMP_FRAME        (DTAPI_E + 57)
#define DTAPI_E_UNSUP_CONV          (DTAPI_E + 58)
#define DTAPI_E_OUTBUF_TOO_SMALL    (DTAPI_E + 59)
#define DTAPI_E_CONFIG              (DTAPI_E + 60)
#define DTAPI_E_TIMEOUT             (DTAPI_E + 61)
#define DTAPI_E_INVALID_TIMEOUT     (DTAPI_E + 62)
#define DTAPI_E_INVALID_FHMODE      (DTAPI_E + 63)
#define DTAPI_E_INVALID_PILOTS      (DTAPI_E + 64)
#define DTAPI_E_INVALID_USEFRAMENO  (DTAPI_E + 65)
#define DTAPI_E_SYMRATE_REQD        (DTAPI_E + 66)
#define DTAPI_E_NO_SYMRATE          (DTAPI_E + 67)
#define DTAPI_E_INVALID_NUMSEGM     (DTAPI_E + 68)
#define DTAPI_E_INVALID_NUMTAPS     (DTAPI_E + 69)
#define DTAPI_E_COMMUNICATION       (DTAPI_E + 70)
#define DTAPI_E_BIND                (DTAPI_E + 71)
#define DTAPI_E_FRAME_INTERVAL      (DTAPI_E + 72)
#define DTAPI_E_INVALID_BWT_EXT     (DTAPI_E + 73)
#define DTAPI_E_INVALID_FFTMODE     (DTAPI_E + 74)
#define DTAPI_E_INVALID_NUMDTSYM    (DTAPI_E + 75)
#define DTAPI_E_INVALID_NUMT2FRM    (DTAPI_E + 76)
#define DTAPI_E_INVALID_SUBCH       (DTAPI_E + 77)
#define DTAPI_E_INVALID_TIME_IL     (DTAPI_E + 78)
#define DTAPI_E_NUM_PLP             (DTAPI_E + 79)
#define DTAPI_E_PLP_NUMBLOCKS       (DTAPI_E + 80)
#define DTAPI_E_NUMPLPS_MUSTBE_1    (DTAPI_E + 81)
#define DTAPI_E_INBAND              (DTAPI_E + 82)
#define DTAPI_E_ISSY                (DTAPI_E + 83)
#define DTAPI_E_OTHER_PLP_IN_BAND   (DTAPI_E + 84)
#define DTAPI_E_CM_NUMPATHS         (DTAPI_E + 85)
#define DTAPI_E_PILOT_PATTERN       (DTAPI_E + 86)
#define DTAPI_E_SUBSLICES           (DTAPI_E + 87)
#define DTAPI_E_NO_GENREF           (DTAPI_E + 88)
#define DTAPI_E_TI_MEM_OVF          (DTAPI_E + 89)
#define DTAPI_E_FEF                 (DTAPI_E + 90)
#define DTAPI_E_UNSUP_FORMAT        (DTAPI_E + 91)
#define DTAPI_E_OUT_OF_SYNC         (DTAPI_E + 92)
#define DTAPI_E_NO_FRAME            (DTAPI_E + 93)
#define DTAPI_E_NO_SUCH_DATA        (DTAPI_E + 94)
#define DTAPI_E_INVALID_TYPE        (DTAPI_E + 95)
#define DTAPI_E_INVALID_MODPARS     (DTAPI_E + 96)
#define DTAPI_E_BIAS_BAL_CELLS      (DTAPI_E + 97)
#define DTAPI_E_COMMON_PLP_COUNT    (DTAPI_E + 98)
#define DTAPI_E_PLP_ID              (DTAPI_E + 99)
#define DTAPI_E_BUFS                (DTAPI_E + 100)
#define DTAPI_E_FIXED_CELL_PARS     (DTAPI_E + 101)
#define DTAPI_E_CM_CHANNEL          (DTAPI_E + 102)
#define DTAPI_E_INVALID_FIFO_IDX    (DTAPI_E + 103)
#define DTAPI_E_INVALID_INP_TYPE    (DTAPI_E + 104)
#define DTAPI_E_INVALID_OUTP_TYPE   (DTAPI_E + 105)
#define DTAPI_E_INVALID_START_FREQ  (DTAPI_E + 106)
#define DTAPI_E_DSLICE_TUNE_POS     (DTAPI_E + 107)
#define DTAPI_E_DSLICE_OFFSETS      (DTAPI_E + 108)
#define DTAPI_E_DSLICE_OVERLAP      (DTAPI_E + 109)
#define DTAPI_E_NOTCH_OFFSETS       (DTAPI_E + 110)
#define DTAPI_E_PLP_BUNDLED         (DTAPI_E + 111)
#define DTAPI_E_BROADBAND_NOTCH     (DTAPI_E + 112)
#define DTAPI_E_L1_PART2_TOO_LONG   (DTAPI_E + 113)
#define DTAPI_E_DSLICE_T1_NDP       (DTAPI_E + 114)
#define DTAPI_E_DSLICE_T1_TSRATE    (DTAPI_E + 115)
#define DTAPI_E_CONNECT_TO_SERVICE  (DTAPI_E + 116)
#define DTAPI_E_INVALID_SYMRATE     (DTAPI_E + 117)
#define DTAPI_E_MODPARS_NOT_SET     (DTAPI_E + 118)
#define DTAPI_E_SERVICE_INCOMP      (DTAPI_E + 119)
#define DTAPI_E_INVALID_LEVEL       (DTAPI_E + 120)
#define DTAPI_E_MODTYPE_UNSUP       (DTAPI_E + 121)
#define DTAPI_E_I2C_LOCK_TIMEOUT    (DTAPI_E + 122)
#define DTAPI_E_INVALID_FREQ        (DTAPI_E + 123)
#define DTAPI_E_INVALID_TSRATESEL   (DTAPI_E + 124)
#define DTAPI_E_INVALID_SPICLKSEL   (DTAPI_E + 125)
#define DTAPI_E_INVALID_SPIMODE     (DTAPI_E + 126)
#define DTAPI_E_NOT_INITIALIZED     (DTAPI_E + 127)
#define DTAPI_E_NOT_LOCKED          (DTAPI_E + 128)
#define DTAPI_E_NO_PERMISSION       (DTAPI_E + 129)
#define DTAPI_E_CANCELLED           (DTAPI_E + 130)
#define DTAPI_E_OUT_OF_RESOURCES    (DTAPI_E + 131)
#define DTAPI_E_LISTEN              (DTAPI_E + 132)
#define DTAPI_E_INVALID_STREAMFMT   (DTAPI_E + 133)
#define DTAPI_E_EVENT_POWER         (DTAPI_E + 134)
#define DTAPI_E_EVENT_REMOVAL       (DTAPI_E + 135)
#define DTAPI_E_UNSUP_ROLLOFF       (DTAPI_E + 136)
#define DTAPI_E_T2_LITE             (DTAPI_E + 137)
#define DTAPI_E_COMP_OVERLAP        (DTAPI_E + 138)
#define DTAPI_E_MULTI_COMPS         (DTAPI_E + 139)
#define DTAPI_E_INVALID_ISI         (DTAPI_E + 140)
#define DTAPI_E_FIRMW_INCOMP        (DTAPI_E + 141)
#define DTAPI_E_INVALID_MODTYPE     (DTAPI_E + 142)
#define DTAPI_E_NO_VIDSTD           (DTAPI_E + 143)
#define DTAPI_E_INVALID_VIDSTD      (DTAPI_E + 144)
#define DTAPI_E_INVALID_AUDSTD      (DTAPI_E + 145)
#define DTAPI_E_INVALID_SCALING     (DTAPI_E + 146)
#define DTAPI_E_INVALID_ROW         (DTAPI_E + 147)
#define DTAPI_E_NOT_STARTED         (DTAPI_E + 148)
#define DTAPI_E_STARTED             (DTAPI_E + 149)
#define DTAPI_E_INVALID_LINE        (DTAPI_E + 150)
#define DTAPI_E_INVALID_STREAM      (DTAPI_E + 151)
#define DTAPI_E_INVALID_ANC         (DTAPI_E + 152)
#define DTAPI_E_INVALID_FRAME       (DTAPI_E + 153)
#define DTAPI_E_NOT_IMPLEMENTED     (DTAPI_E + 154)
#define DTAPI_E_INVALID_CHANNEL     (DTAPI_E + 155)
#define DTAPI_E_INVALID_GROUP       (DTAPI_E + 156)
#define DTAPI_E_INVALID_FORMAT      (DTAPI_E + 157)
#define DTAPI_E_INVALID_FIELD       (DTAPI_E + 158)
#define DTAPI_E_BUF_TOO_LARGE       (DTAPI_E + 159)
#define DTAPI_E_INVALID_DELAY       (DTAPI_E + 160)
#define DTAPI_E_EXCL_MANDATORY      (DTAPI_E + 161)
#define DTAPI_E_INVALID_ROLLOFF     (DTAPI_E + 162)
#define DTAPI_E_CM_UNSUP            (DTAPI_E + 163)
#define DTAPI_E_I2C                 (DTAPI_E + 164)
#define DTAPI_E_STATE               (DTAPI_E + 165)
#define DTAPI_E_NO_LOCK             (DTAPI_E + 166)
#define DTAPI_E_RANGE               (DTAPI_E + 167)
#define DTAPI_E_INVALID_T2PROFILE   (DTAPI_E + 168)
#define DTAPI_E_DSLICE_ID           (DTAPI_E + 169)
#define DTAPI_E_EXCL_ACCESS_REQD    (DTAPI_E + 170)
#define DTAPI_E_CHAN_ALREADY_ADDED  (DTAPI_E + 171)
#define DTAPI_E_LAYER_ID            (DTAPI_E + 172)
#define DTAPI_E_INVALID_FECMODE     (DTAPI_E + 173)
#define DTAPI_E_INVALID_PORT        (DTAPI_E + 174)
#define DTAPI_E_INVALID_PROTOCOL    (DTAPI_E + 175)
#define DTAPI_E_INVALID_FEC_MATRIX  (DTAPI_E + 176)
#define DTAPI_E_INVALID_IP_ADDR     (DTAPI_E + 177)
#define DTAPI_E_INVALID_SRCIP_ADDR  (DTAPI_E + 178)
#define DTAPI_E_IPV6_NOT_SUPPORTED  (DTAPI_E + 179)
#define DTAPI_E_INVALID_DIFFSERV    (DTAPI_E + 180)
#define DTAPI_E_INVALID_FOR_ACM     (DTAPI_E + 181)
#define DTAPI_E_NWAP_DRIVER         (DTAPI_E + 182)
#define DTAPI_E_INIT_ERROR          (DTAPI_E + 183)
#define DTAPI_E_NOT_USB3            (DTAPI_E + 184)
#define DTAPI_E_INSUF_BW            (DTAPI_E + 185)
#define DTAPI_E_NO_ANC_DATA         (DTAPI_E + 186)
#define DTAPI_E_MATRIX_HALTED       (DTAPI_E + 187)
#define DTAPI_E_VLAN_NOT_FOUND      (DTAPI_E + 188)
#define DTAPI_E_NO_ADAPTER_IP_ADDR  (DTAPI_E + 189)
#define DTAPI_E_INVALID_BTYPE       (DTAPI_E + 190)
#define DTAPI_E_INVALID_PARTIAL     (DTAPI_E + 191)
#define DTAPI_E_INVALID_NUMTS       (DTAPI_E + 192)
#define DTAPI_E_INVALID             (DTAPI_E + 193)
#define DTAPI_E_NO_RS422            (DTAPI_E + 194)
#define DTAPI_E_FECFRAMESIZE        (DTAPI_E + 195)

//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=
//=+=+=+=+=+=+=+=+ DVB-C2, DVB-S2, DVB-T2, ISDB-Tmm Multi PLP Parameters +=+=+=+=+=+=+=+=+
//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtBigTsSplitPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying 'Big TS' splitting
//
struct DtBigTsSplitPars
{
    bool  m_Enabled;                // Enable 'Big TS' splitting
    bool  m_IsCommonPlp;            // Is Common PLP or Layer
    bool  m_SplitSdtIn;             // SDT already split (e.g. BBC TS-files)
    std::vector<int>  m_Pids;       // Series of PIDs to include in the PLP

    // The parameters below are not used in case of a common PLP
    int  m_OnwId;                   // Original Network ID of the Big-TS
    int  m_TsId;                    // Transport Stream ID of the Big-TS
    int  m_ServiceId;               // ID of service to include in PLP
    int  m_PmtPid;                  // PID of the PMT-table
    int  m_NewTsId;                 // New Transport Stream ID
    int  m_SdtLoopDataLength;       // SDT loop data length. 0..168
                                    // Not used if m_SplitSdtIn == true
    // The SDT-Actual loop data
    unsigned char  m_SdtLoopData[168];

public:
    // Methods
    void  Init(void);

    // Operators
    bool  operator == (DtBigTsSplitPars& TsSplitPars);
    bool  operator != (DtBigTsSplitPars& TsSplitPars);
    bool  IsEqual(DtBigTsSplitPars& TsSplitPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtPlpInpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying the PLP input
//
struct DtPlpInpPars
{
    // PLP input data types
    enum InDataType 
    {
        TS188,                      // 188-byte TS packets 
        TS204,                      // 204-byte TS packets
        GSE                         // Generic Stream Encapsulation
    };

public:
    int  m_FifoIdx;                 // Fifo index used for the PLP
                                    // If Big TS file splitting is used, PLPs in a group
                                    // can share the input FIFO
    InDataType  m_DataType;         // TS188 or TS204 byte packets
    DtBigTsSplitPars  m_BigTsSplit; // Big TS splitting (optional)

public:
    // Methods
    void  Init(int Idx = 0);

    // Operators
    bool  operator == (DtPlpInpPars& PlpInPars);
    bool  operator != (DtPlpInpPars& PlpInPars);
    bool  IsEqual(DtPlpInpPars& PlpInPars);
};

// Test point data format
#define DTAPI_TP_FORMAT_HEX         0
#define DTAPI_TP_FORMAT_BIT         1
#define DTAPI_TP_FORMAT_CFLOAT32    2
#define DTAPI_TP_FORMAT_INT64       3

// Complex floating point type
struct DtComplexFloat
{
    float  m_Re, m_Im;              // Real, imaginary part
};

// Function to write test point data
typedef void  DtTpWriteDataFunc(void* pOpaque, int TpIndex, int StreamIndex,
                  const void* Buffer, int Length, int Format, float Mult, int IsNewFrame);

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtTestPointOutPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for enabling the test point data output specifying a callback function
//
struct DtTestPointOutPars
{
public:
    bool  m_Enabled;                // Enable TP data output
    void*  m_pTpWriteDataOpaque;    // Opaque data pointer passed to TpWriteData

    // Callback function for writing test data
    DtTpWriteDataFunc*  m_pTpWriteDataFunc;

public:
    // Methods
    void  Init(void);

    // Operators
    bool  operator == (DtTestPointOutPars& RbmPars);
    bool  operator != (DtTestPointOutPars& RbmPars);
    bool  IsEqual(DtTestPointOutPars& RbmPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtVirtualOutData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
// 
// Structure describing and containing the output data for a virtual output channel 
// 
struct DtVirtualOutData
{
    // Virtual output data types
    enum OutDataType 
    {
        IQ_INT16,                   // 16-bit I/Q samples 
        IQ_FLOAT32,                 // 32 bit float I/Q samples
        T2MI_TS188,                 // 188 byte T2MI TS-packets
        DVBS2_L3,                   // L3 format for DVB-S2
    };
    OutDataType  m_DataType;        // Output data type

    // Output type dependent parameters
    union {
        // 16-bit int I/Q samples
        struct {
            // Array of buffers; Per output channel a buffer with samples
            const unsigned char**  m_pBuffer; 
            int  m_NumBuffers;      // Number of buffers
            int  m_NumBytes;        // Number of bytes
        } IqSamplesInt16;

        // 32-bit float I/Q samples
        struct {
            // Array of buffers. Per output channel a buffer with samples
            const unsigned char**  m_pBuffer;
            int  m_NumBuffers;      // Number of buffers
            int  m_NumBytes;        // Number of bytes
        } IqSamplesFloat32;

        // 188-byte T2MI TS packets
        struct {
            // Pointer to T2MI TS-packet(s)
            const unsigned char*  m_pBuffer;
            int  m_NumBytes;        // Number of bytes
            __int64  m_T2MiFrameNr; // T2MI super frame number counter
        } T2MiTs188;

        // L3 frames
        struct {
            // Pointer to L3 frame(s)
            const unsigned char*  m_pBuffer;
            int  m_NumBytes;        // Number of bytes
        } DvbS2L3;
        
    } u;
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtVirtualOutPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying the output type and level for a virtual output.
// These parameters are only relevant when outputting to a virtual output channel.
//
struct DtVirtualOutPars
{
    bool  m_Enabled;                // Enable virtual output parameters
    DtVirtualOutData::OutDataType  m_DataType;
                                    // Type of output data 
    double  m_Gain;                 // The square root of the average power of
                                    // the generated signal
public:
    void  Init(void);
    bool  IsEqual(DtVirtualOutPars& VirtOutPars);
    bool  operator == (DtVirtualOutPars& VirtOutPars);
    bool  operator != (DtVirtualOutPars& VirtOutPars);
};



//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DAB Parameters +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=

// DAB error protection modes
#define DTAPI_DAB_UEP               0            // DAB unequal error protection
#define DTAPI_DAB_EEP               1            // DAB equal error protection

// DAB data extraction mode
enum DtDabExtractionMode
{
    DAB_RAW,                        // Raw DAB stream, no extraction
    DAB_EXTRACTION_AAC,             // AAC/DAB+ stream extraction 
    DAB_EXTRACTION_DMB              // DMB stream extraction
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabEtiStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for DAB Ensemble Transport Interface (ETI) selection
//
struct DtDabEtiStreamSelPars
{
    // No selection parameters yet
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Structure for DAB selection
//
struct DtDabStreamSelPars
{
    int  m_BitrateKbps;             // Bitrate in kbps
    int  m_ErrProtLevel;            // Error protection level: 1..5 (UEP); 1..4 (EEP)
    int  m_ErrProtMode;             // Error protection mode: DTAPI_DAB_UEP/EEP
    int  m_ErrProtOption;           // Error protection option (EEP only): 0 or 1
    int  m_StartAddress;            // Start address in DAB capacity units (64bits)

    // DAB data extraction mode: DAB_RAW, DAB_EXTRACTION_AAC or DAB_EXTRACTION_DMB
    DtDabExtractionMode  m_ExtractionMode;
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabFicStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Selection parameters for a DAB Fast Information Channel (FIC)
//
struct DtDabFicStreamSelPars
{
    // Parameters below are passed in the WriteStreamFunc() callback function;
    // the parameters are not used for selection
    int  m_CifIndex;                // Index of the CIF in the DAB frame to which this
                                    // FIB is associated
    int  m_FibIndex;                // Index of this FIB in the group of FIBs that 
                                    // are associated to the same CIF
};

// Forward declarations
struct DtDabService;
struct DtDabSubChannel;
struct DtDabServiceComp;

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabEnsembleInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Information about a DAB ensemble
//
struct DtDabEnsembleInfo
{
    int  m_CountryId;               // Country identifier
    int  m_EnsembleReference;       // Indentifier of this ensemble in national area
    int  m_ExtCountryCode;          // Extended country code
    int  m_InterTableId;            // International table identifier
    std::wstring  m_Label;          // Label identifying this ensemble
    int  m_LocalTimeOffset;         // Local time offset in half hours from UTC
    int  m_LtoUnique;               // Covers one(=0) or several(=1) time zones
    int  m_TransmissionMode;        // Transmission mode: 1..4

    // Services contained in this ensemble
    std::vector<DtDabService>  m_Services;

    // Subchannels in this ensemble. Key: subchannel identifier
    std::map<int, DtDabSubChannel>  m_SubChannels;
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabService -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Information about a single service. Every service must have one primary service
// component and can have one or more non-primay service components
//
struct DtDabService
{
    int  m_CondAccessId;            // Conditional access identifier
    int  m_CountryId;               // Country identifier
    int  m_ExtCountryCode;          // Extended country code; -1 for program service
    bool  m_IsLocal;                // True if local (partial) ensemble service area
    std::wstring  m_Label;          // Label identifying this service 
    int  m_ServiceReference;        // Identifier of the service

    // Components in this service
    std::vector<DtDabServiceComp>  m_Components;
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabServiceComp -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Information about a single DAB service component
//
struct DtDabServiceComp
{
    int  m_AudioServiceCompType;    // Audio service component type; -1 if not applicable
    int  m_DataServiceCompType;     // Data service component type; -1 if not applicable
    int  m_FidChannelId;            // Fast information data channel identifier 0..63;
                                    // -1 if not applicable
    bool  m_HasCondAccess;          // True if access control applies
    bool  m_IsPrimary;              // True if this is the primary component
    std::wstring  m_Label;          // Label identifying this service component
    int  m_Language;                // Service compoment language or -1
    int  m_SubChannelId;            // Subchannel identifier: 0..63; -1 if not applicable
    int  m_ServiceCompId;           // Service component identifier; -1 if not applicable
    int  m_TransportMechanismId;    // Transport mechanism identifier
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDabSubChannel -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// A DAB subchannel contains the data for a single audio or data stream. Every service
// component refers to a subchannel, but multiple service components can refer to the
// same subchannel. In that case more than one service can use the same audio stream.
//
struct DtDabSubChannel
{
    int  m_BitrateKbps;             // Bitrate in kbps
    int  m_ErrProtLevel;            // Protection level UEP: 1..5; EEP: 1..4
    int  m_ErrProtMode;             // Error protection mode: DTAPI_DAB_UEP/EEP
    int  m_ErrProtOption;           // Option for EEP; -1 for UEP
    int  m_FecScheme;               // FEC scheme; -1 if not applicable
    int  m_StartAddress;            // Start address in capacity units (64bits)
    int  m_SubChannelId;            // Subchannel identifier: 0..63
    int  m_SubChannelSize;          // Size of subchannel in capacity units (64bits)
    int  m_UepTableIndex;           // Index in UEP table; -1 if not applicable
    int  m_UepTableSwitch;          // UEP table switch; -1 if not applicable
};

//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DVB-C2 Parameters +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

// Maxima
#define DTAPI_DVBC2_NUM_DSLICE_MAX  255          // Maximum number of data slices
#define DTAPI_DVBC2_NUM_PLP_MAX     255          // Maximum number of PLPs
#define DTAPI_DVBC2_NUM_NOTCH_MAX   16           // Maximum number of Notches

// PLP IDs
#define DTAPI_DVBC2_PLP_ID_NONE     -1           // No PLP selected
#define DTAPI_DVBC2_PLP_ID_AUTO     -2           // Automatic PLP selection

// Data slice IDs
#define DTAPI_DVBC2_DSLICE_ID_AUTO  -2           //  Automatic data slice selection

// m_Bandwidth
#define DTAPI_DVBC2_6MHZ            6            // 6 MHz
#define DTAPI_DVBC2_8MHZ            8            // 8 MHz

// m_Guard - Guard interval
#define DTAPI_DVBC2_GI_1_128        0            // 1/128
#define DTAPI_DVBC2_GI_1_64         1            // 1/64

// m_L1TiMode - L1 time interleaving mode
#define DTAPI_DVBC2_L1TIMODE_NONE   0            // No time interleaving
#define DTAPI_DVBC2_L1TIMODE_BEST   1            // Best fit
#define DTAPI_DVBC2_L1TIMODE_4      2            // 4 OFDM symbols 
#define DTAPI_DVBC2_L1TIMODE_8      3            // 8 OFDM symbols 

// m_Type - PLP type
#define DTAPI_DVBC2_PLP_TYPE_COMMON 0            // Common PLP
#define DTAPI_DVBC2_PLP_TYPE_GROUPED 1           // Grouped Data PLP
#define DTAPI_DVBC2_PLP_TYPE_NORMAL 2            // Normal Data PLP

// m_FecType - PLP FEC type
#define DTAPI_DVBC2_LDPC_16K        0            // 16K LDPC
#define DTAPI_DVBC2_LDPC_64K        1            // 64K LDPC

// m_CodeRate - PLP code rate
#define DTAPI_DVBC2_COD_2_3         1            // 2/3
#define DTAPI_DVBC2_COD_3_4         2            // 3/4
#define DTAPI_DVBC2_COD_4_5         3            // 4/5
#define DTAPI_DVBC2_COD_5_6         4            // 5/6
#define DTAPI_DVBC2_COD_8_9         5            // 8/9 (For 16K FEC)
#define DTAPI_DVBC2_COD_9_10        5            // 9/10 (For 64K FEC)

// m_Modulation - PLP constellation
#define DTAPI_DVBC2_QAM16           1            // 16-QAM
#define DTAPI_DVBC2_QAM64           2            // 64-QAM
#define DTAPI_DVBC2_QAM256          3            // 256-QAM
#define DTAPI_DVBC2_QAM1024         4            // 1024-QAM
#define DTAPI_DVBC2_QAM4096         5            // 4096-QAM
#define DTAPI_DVBC2_QAM16384        6            // 16K-QAM non standard, but supported
#define DTAPI_DVBC2_QAM65536        7            // 64K-QAM non standard, but supported

// m_Issy - PLP ISSY
#define DTAPI_DVBC2_ISSY_NONE       0            // No ISSY field is used
#define DTAPI_DVBC2_ISSY_SHORT      1            // 2 byte ISSY field is used
#define DTAPI_DVBC2_ISSY_LONG       2            // 3 byte ISSY field is used

// m_TiDepth - Data slice time interleaving depth
#define DTAPI_DVBC2_TIDEPTH_NONE    0            // No time interleaving
#define DTAPI_DVBC2_TIDEPTH_4       1            // 4 OFDM symbols 
#define DTAPI_DVBC2_TIDEPTH_8       2            // 8 OFDM symbols 
#define DTAPI_DVBC2_TIDEPTH_16      3            // 16 OFDM symbols 

// m_Type - Data slice type
#define DTAPI_DVBC2_DSLICE_TYPE_1   0            // Type 1 (Single PLP and fixed mod/cod)
#define DTAPI_DVBC2_DSLICE_TYPE_2   1            // Type 2 

// m_FecHdrType - Data slice FEC frame header type
#define DTAPI_DVBC2_FECHDR_TYPE_ROBUST 0         // Robust mode
#define DTAPI_DVBC2_FECHDR_TYPE_HEM 1            // High efficiency mode

// DVB-C2 Test points
enum {
    DTAPI_DVBC2_TP07,               // FEC frame
    DTAPI_DVBC2_TP08,
    DTAPI_DVBC2_TP10,
    DTAPI_DVBC2_TP13,
    DTAPI_DVBC2_TP15,               // Data slice
    DTAPI_DVBC2_TP18,               // OFDM output
    DTAPI_DVBC2_TP20,
    DTAPI_DVBC2_TP22,               // FEC header
    DTAPI_DVBC2_TP26,
    DTAPI_DVBC2_TP27,               // L1 header
    DTAPI_DVBC2_TP31,
    DTAPI_DVBC2_TP32,               // L1 part2 data
    DTAPI_DVBC2_TP33,               // L1 part2 + padding & CRC
    DTAPI_DVBC2_TP37,
    DTAPI_DVBC2_TP40,
    DTAPI_DVBC2_TP41,
    DTAPI_DVBC2_TP42,
    DTAPI_DVBC2_TP01,
    DTAPI_DVBC2_TP_COUNT,
};

// DVB-C2 test points
extern const int DTAPI_DVBC2_TESTPOINTS[DTAPI_DVBC2_TP_COUNT];

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DSlicePars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying the data slice parameters
//
struct DtDvbC2DSlicePars
{
    int  m_Id;                      // Data slice ID. 0..255
    int  m_TunePosition;            // Tune position relative to the start frequency in 
                                    // multiples of pilot carrier spacing.
                                    // For guard interval 1/128: 0..8191 
                                    // For guard interval 1/64: 0..16383 
    int  m_OffsetLeft;              // Offset left in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: -128..127 
                                    // For guard interval 1/64: -256..255 
    int  m_OffsetRight;             // Offset right in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: -128..127 
                                    // For guard interval 1/64: -256..255 
                                    // If m_OffsetLeft = m_OffsetRight, the data slice is
                                    // empty and no input streams are created for the PLPs
                                    // of the data slice.
    int  m_TiDepth;                 // Time interleaving depth. See DTAPI_DVBC2_TIDEPTH_x
    int  m_Type;                    // Data slice type. See DTAPI_DVBC2_DSLICE_TYPE_x
    int  m_FecHdrType;              // FEC header type. See DTAPI_DVBC2_FECHDR_TYPE_x 
    bool  m_ConstConfig;            // Constant data slice configuration
    bool  m_LeftNotch;              // Left notch present
    
    // Data per PLP
    std::vector<DtDvbC2PlpPars>  m_Plps;

public:
    void  Init(int Id=0);
    bool  IsEqual(DtDvbC2DSlicePars& DSlicePars);
    bool  operator == (DtDvbC2DSlicePars& DSlicePars);
    bool  operator != (DtDvbC2DSlicePars& DSlicePars);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2L1UpdatePlpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// struct for L1 PLP parameter update support
//
struct DtDvbC2L1UpdatePlpPars 
{
    bool  m_Enable;                 // Enable or disable the PLP.
                                    // Only PLPs with m_NoData = true can be disabled.
public:
    void  Init();
    bool  IsEqual(DtDvbC2L1UpdatePlpPars& PlpUpdatePars);
    bool  operator == (DtDvbC2L1UpdatePlpPars& PlpUpdatePars);
    bool  operator != (DtDvbC2L1UpdatePlpPars& PlpUpdatePars);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2L1UpdateDSlicePars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// struct for L1 DSlice parameter update support
//
struct DtDvbC2L1UpdateDSlicePars
{
    bool  m_Enable;                 // Enable or disable the DSlice.
                                    // Only dummy data slices can be disabled. 
                                    // A data slice is considered as dummy if either:
                                    // - m_OffsetLeft == m_OffsetRight in its 
                                    //   global configuration; or
                                    // - all its PLPs have m_NoData = true
                                    // A dummy data slice is modulated with dummy data.

    int  m_OffsetLeft;              // Data slice left offset - 0..2^(8+g)-1
    int  m_OffsetRight;             // Data slice right offset - 0..2^(8+g)-1
                                    // If the data slice is not dummy:
                                    // - For type 1 , no size change is accepted.
                                    // - For type 2 , size change is accepted 
                                    //   provided it is not zero (i.e. m_OffsetRight >
                                    // m_OffsetLeft). It is up to the user to ensure that
                                    // there is no bitrate overflow.
    std::vector<DtDvbC2L1UpdatePlpPars>  m_Plps; // L1 PLP updates

public:
    void  Init();
    bool  IsEqual(DtDvbC2L1UpdateDSlicePars& DSliceUpdatePars);
    bool  operator == (DtDvbC2L1UpdateDSlicePars& DSliceUpdatePars);
    bool  operator != (DtDvbC2L1UpdateDSlicePars& DSliceUpdatePars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2L1UpdatePars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// L1 update support
//
struct DtDvbC2L1UpdatePars
{
    int  m_NumFrames;               // The following parameters are used during
                                    // 'm_NumFrames' C2 frames
    // L1 DSlice updates
    std::vector<DtDvbC2L1UpdateDSlicePars>  m_DSlices;

public:
    void  Init();
    bool  IsEqual(DtDvbC2L1UpdatePars& L1UpdatePars);
    bool  operator == (DtDvbC2L1UpdatePars& L1UpdatePars);
    bool  operator != (DtDvbC2L1UpdatePars& L1UpdatePars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2ModStatus -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for retrieving the DVB-C2 MPLP modulator status
//
struct DtDvbC2ModStatus
{
    int  m_MplpModFlags;            // Multi PLP modulator flags
    __int64  m_DjbOverflows;        // Count number of DJB overflows. If it happens,
                                    // issy output delay must be decreased or "issy bufs"
                                    // increased.
    __int64  m_DjbUnderflows;       // Count number of DJB underflows. If it happens,
                                    // issy output delay must be increased.
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2NotchPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying the notch parameters
//
struct DtDvbC2NotchPars
{
    int  m_Start;                   // Notch start in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: 0..8191 
                                    // For guard interval 1/64: 0..16383 
    int  m_Width;                   // Notch width in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: 0..255 
                                    // For guard interval 1/64: 0..511 
public:
    void  Init(void);
    bool  IsEqual(DtDvbC2NotchPars& NotchPars);
    bool  operator == (DtDvbC2NotchPars& NotchPars);
    bool  operator != (DtDvbC2NotchPars& NotchPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2PaprPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying and enabling the PAPR reduction parameters
//
struct DtDvbC2PaprPars
{
    bool  m_TrEnabled;              // TR enabled
    double  m_TrVclip;              // TR clipping threshold 1..4.32 (Volt)
    int  m_TrMaxIter;               // TR maximum number of iterations. Must be >= 1.
                                    // Note: PAPR TR processing time is proportional
                                    //       to this parameter
public:
    void  Init(void);
    bool  IsEqual(DtDvbC2PaprPars& PaprPars);
    bool  operator == (DtDvbC2PaprPars& PaprPars);
    bool  operator != (DtDvbC2PaprPars& PaprPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2ParamInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// DVB-C2 parameter info
//
struct DtDvbC2ParamInfo 
{
    int  m_L1Part2Length;           // #bits of the L1 part2 data (including CRC)
    int  m_NumL1Symbols;            // Number of L1 symbols (LP)
    int  m_NumSymbols;              // Total number of symbols per frame (LP + Ldata)
    int  m_PilotSpacing;            // Distance between pilots (Dx)
    int  m_FftSize;                 // FFT size
    int  m_MinCarrierOffset;        // Lowest used carrier offset (Koffset)
    int  m_CenterFrequency;         // Center frequency in multiples of the carrier 
                                    // spacing
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2PlpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying the parameters of a PLP.
// In case of bundled PLPs, only the mode adaptation parameters from the 
// first PLP of the bundle are used.
//
struct DtDvbC2PlpPars
{
    bool  m_Hem;                    // High Efficiency Mode
    bool  m_Npd;                    // Null Packet Deletion
    int  m_Issy;                    // Issy mode. See DTAPI_DVBC2_ISSY_x
    int  m_IssyBufs;                // Issy BUFS
    int  m_IssyOutputDelay;         // Delay (in T units) between the incoming data and
                                    // the output TS in the receiver model. This value 
                                    // determines the minimum and maximum dejitter buffer
                                    // usage and is used to compute the ISSY BUFSTAT field
    int  m_TsRate;                  // If 0 the rate is computed from the PLP parameters,
                                    // only possible in case of single PLP and no ISSY 
                                    // is used
    int  m_Ccm;                     // ACM/CMM bit in the BBframe header 0 or 1
    int  m_Id;                      // PLP ID. 0..255
    bool  m_Bundled;                // A bundled PLP can appear in several data slices. 
                                    // All the PLP instances have the same PLP ID.
                                    // Only a single input stream results from the 
                                    // first PLP of the bundle.
    int  m_Type;                    // PLP Type. See DTAPI_DVBC2_PLP_TYPE_x
    int  m_GroupId;                 // Group ID. 0..255
    int  m_FecType;                 // FEC Type. 0=16K, 1=64K
    int  m_CodeRate;                // PLP Code rate. See DTAPI_DVBC2_COD_x
    int  m_Modulation;              // PLP Modulation. See DTAPI_DVBC2_x
    int  m_HdrCntr;                 // Header counter #FECFrames following the 
                                    // FECFrame header. 0=1FF 1=2FF.
                                    // Only used for DSlice type 2.

    // ACM test mode. Only available for type 2 data slices. If number ACM headers != 0, 
    // then the successive XFEC frames of this PLP use the modulation and coding 
    // parameters taken from the m_AcmHeaders array. After the last value is used, it
    // loops again to the start of the array.
    // So DtDvbC2PlpPars.m_FecType, m_Modulation, m_CodeRate and m_HdrCntr are ignored.
    std::vector<DtDvbC2XFecFrameHeader> m_AcmHeaders;

    bool  m_PsiSiReproc;            // Indicates whether PSI/SI reprocessing is performed
    int  m_TsId;                    // Transport Stream ID (if m_PsiSiReproc=false)
    int  m_OnwId;                   // Original Network ID (if m_PsiSiReproc=false)
    bool  m_NoData;                 // No input data is provided for this PLP. 
                                    // It is implicitely true for all PLPs in a data slice 
                                    // with m_OffsetLeft = m_OffsetRight
public:
    void  Init(int PlpId = 0);
    bool  IsEqual(DtDvbC2PlpPars& PlpPars);
    bool  operator == (DtDvbC2PlpPars& PlpPars);
    bool  operator != (DtDvbC2PlpPars& PlpPars);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2XFecFrameHeader -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// DVBC2 FEC frame header for ACM test
//
struct DtDvbC2XFecFrameHeader
{
    int  m_FecType;                 // PLP FEC Type. 0=16K, 1=64K
    int  m_Modulation;              // PLP Modulation. See DTAPI_DVBC2_x
    int  m_CodeRate;                // PLP Code rate. See DTAPI_DVBC2_COD_x
    int  m_HdrCntr;                 // Header counter #FEC frames following the 
                                    // FEC frame header. 0=1FF, 1=2FF
    int  m_XFecFrameCount;          // 1..256: Number of successive XFEC frames using 
                                    // these parameters
public:
    void  Init(void);
    bool  IsEqual(DtDvbC2XFecFrameHeader& FecHeader);
    bool  operator == (DtDvbC2XFecFrameHeader& FecHeader);
    bool  operator != (DtDvbC2XFecFrameHeader& FecHeader);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2Pars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying the DVB-C2 modulation parameters
//
 struct DtDvbC2Pars
{
public:
    // General C2 parameters
    int  m_Bandwidth;               // Bandwidth. Defines the OFDM carrier spacing 
                                    // F=8e6*bandwidth/7/4096 Hz. See DVBC2_BW_x
    int  m_NetworkId;               // Network ID.  0..0xFFFF
    int  m_C2SystemId;              // C2 System ID.  0..0xFFFF
    int  m_StartFrequency;          // Start frequency in multiple of carrier spacing
                                    // 0..2^24 - 1 and multiples of dx. 
                                    // For guard interval 1/128 dx=24, otherwise dx=12
    int  m_C2Bandwidth;             // Bandwidth of the generated signal in 
                                    // multiples of pilot carrier spacing. 0..65535
    int  m_GuardInterval;           // Guard interval See DVBC2_GI_x
    bool  m_ReservedTone;           // Reserved tone. When there are reserved carriers
                                    // (e.g. PAPR TR is enabled) it shall be set to true; 
                                    // otherwise false
    int  m_L1TiMode;                // L1 time interleaving mode. See DVBC2_L1TIMODE_x

    // Data slices parameters
    int  m_NumDSlices;              // Number of data slices
    DtDvbC2DSlicePars  m_DSlices[DTAPI_DVBC2_NUM_DSLICE_MAX];

    // Notches
    int  m_NumNotches;              // Number of notches
    DtDvbC2NotchPars  m_Notches[DTAPI_DVBC2_NUM_NOTCH_MAX];
    
    // PLP input
    int  m_NumPlpInputs;            // Number of PLPs
    DtPlpInpPars  m_PlpInputs[DTAPI_DVBC2_NUM_PLP_MAX];  // PLP inputs (Optional)

    DtDvbC2PaprPars  m_PaprPars;    // PAPR Params (Optional)
    DtVirtualOutPars  m_VirtOutput; // Virtual Output parameters(Optional) 
    DtTestPointOutPars  m_TpOutput; // Test point data output parameters (Optional)
    int  m_OutpFreqOffset;          // Output frequency offset from 'm_StartFrequency'
                                    // in carriers of the generated spectrum. 
                                    // Must be multiple of dx.
    int  m_OutpBandwidth;           // Output bandwidth in carriers and a multiple of dx.
                                    // 0 means default output bandwidth.  
                                    // Note: for convenience, one more carrier is output
                                    // if an edge carrier needs to be output.

    std::vector<DtDvbC2L1UpdatePars>  m_L1Updates; // L1 updates

    // Undocumented
    int  m_L1P2ChangeCtr;           // Undocumented. For internal use only
    bool  m_NotchTestEnable;        // Undocumented. For internal use only
    int  m_TimeWindowLength;        // Undocumented. For internal use only

public:
    DTAPI_RESULT  CheckValidity(void);
    DTAPI_RESULT  GetParamInfo(DtDvbC2ParamInfo& C2Info);
    void  Init(void);
    bool  IsEqual(DtDvbC2Pars& C2Pars);
    bool  operator == (DtDvbC2Pars& C2Pars);
    bool  operator != (DtDvbC2Pars& C2Pars);
};
 
//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DVB-C2 Demodulation  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

// DtDvbC2DemodL1Part2Plp::m_PayloadType - The PLP payload type
#define DTAPI_DVBC2_PAYLOAD_GFPS    0            // Generic fixed-length packetized stream
#define DTAPI_DVBC2_PAYLOAD_GCS     1            // Generic continuous stream
#define DTAPI_DVBC2_PAYLOAD_GSE     2            // Generic stream encapsulation
#define DTAPI_DVBC2_PAYLOAD_TS      3            // Transport Stream

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DemodL1PlpSigDataPlp -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
 //
// Struct for storing layer 1 PLP signalling information per PLP.
// For type 1 data slices this struct contains the PLP-signalling information
// from the layer 1 part 2 signalling.
// For type 2 data slices this struct contains the PLP-signalling information
// from the layer 1 part 1 (=FEC-frame header).
//
struct DtDvbC2DemodL1PlpSigDataPlp
{
    int  m_Id;                      // PLP ID: 0..255
    int  m_FecType;                 // PLP FEC type: 0=LDPC 16K, 1=LDPC 64K
    int  m_Modulation;              // PLP modulation, see DTAPI_DVBC2_x
    int  m_CodeRate;                // PLP modulation, see DTAPI_DVBC2_x
    int  m_HdrCntr;                 // Header counter #FEC frames following the 
                                    // FEC frame header. 0=1FF, 1=2FF. Only present for
                                    // type 2 data slices

    DtDvbC2DemodL1PlpSigDataPlp();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DemodL1PlpSigData -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Struct for storing the DVB-C2 layer 1 PLP signalling data
//
struct DtDvbC2DemodL1PlpSigData
{
    int  m_NumPlps;                 // Number of PLPs
    std::vector<DtDvbC2DemodL1PlpSigDataPlp>  m_Plps;

    DtDvbC2DemodL1PlpSigData();
    
    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);

};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DemodL1Part2Plp -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Struct for storing Layer 1 part 2 information per PLP
//
struct DtDvbC2DemodL1Part2Plp
{
    int  m_Id;                      // PLP ID: 0..255
    int  m_Bundled;                 // Bundled PLP
    int  m_Type;                    // PLP type, see DTAPI_DVBC2_PLP_TYPE_x
    int  m_PayloadType;             // PLP payload type: 0..3
    int  m_GroupId;                 // Group ID: 0..255
    // Start, FecType, Modulation and CodeRate parameters are not present for type 2 data 
    // slices
    int  m_Start;                   // PLP start: Start of the first complete XFECframe
    int  m_FecType;                 // PLP FEC type: 0=LDPC 16K, 1=LDPC 64K
    int  m_Modulation;              // PLP modulation, see DTAPI_DVBC2_x
    int  m_CodeRate;                // PLP modulation, see DTAPI_DVBC2_x
    int  m_PsiSiReproc;             // Indicates whether PSI/SI reprocessing is performed
    int  m_TsId;                    // Transport Stream ID (if m_PsiSiReproc=false)
    int  m_OnwId;                   // Original Network ID (if m_PsiSiReproc=false)
 
    DtDvbC2DemodL1Part2Plp();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};
 
//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DemodL1Part2DSlice -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Struct for storing Layer 1 part 2 information per data slice
//
struct DtDvbC2DemodL1Part2DSlice 
{
    int  m_Id;                      // Data slice ID: 0..255
    int  m_TunePosition;            // Tune position relative to the start frequency in 
                                    // multiples of pilot carrier spacing.
                                    // For guard interval 1/128: 0..8191 
                                    // For guard interval 1/64: 0..16383 
    int  m_OffsetLeft;              // Offset left in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: -128..127 
                                    // For guard interval 1/64: -256..255 
    int  m_OffsetRight;             // Offset right in multiples of pilot carrier spacing.
                                    // For guard interval 1/128: -128..127 
                                    // For guard interval 1/64: -256..255 
                                    // If m_OffsetLeft = m_OffsetRight, the data slice is
                                    // empty and no input streams are created for the PLPs
                                    // of the data slice.
    int  m_TiDepth;                 // Time interleaving depth, see DTAPI_DVBC2_TIDEPTH_x
    int  m_Type;                    // Data slice type, see DTAPI_DVBC2_DSLICE_TYPE_x
    int  m_FecHdrType;              // FEC header type, see DTAPI_DVBC2_FECHDR_TYPE_x 
    int  m_ConstConfig;             // Constant data slice configuration flag
    int  m_LeftNotch;               // Left notch present flag
    // PLPs
    int  m_NumPlps;                 // Number of PLPs
    std::vector<DtDvbC2DemodL1Part2Plp>  m_Plps;

    DtDvbC2DemodL1Part2DSlice();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2DemodL1Part2Data -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Struct for storing the DVB-C2 layer 1 part 2data
//
struct DtDvbC2DemodL1Part2Data
{
    int  m_NetworkId;               // Network ID:  0..0xFFFF
    int  m_C2SystemId;              // C2 System ID: 0..0xFFFF
    int  m_StartFrequency;          // Start frequency in multiple of carrier spacing:
                                    // 0..2^24 - 1
    int  m_C2Bandwidth;             // Bandwidth of the generated signal in 
                                    // multiples of pilot carrier spacing: 0..65535
    int  m_GuardInterval;           // Guard interval. See DVBC2_GI_x
    int  m_C2FrameLength;           // C2 frame length: #Data symbols per C2 frame
    int  m_L1P2ChangeCtr;           // Value of the L1_PART2_CHANGE_COUNTER field
    int  m_ReservedTone;            // Reserved tone

    // Data slices
    int  m_NumDSlices;              // Number of data slices
    std::vector<DtDvbC2DemodL1Part2DSlice>  m_DSlices;
 
    // Notches
    int  m_NumNotches;              // Number of notches
    std::vector<DtDvbC2NotchPars>  m_Notches;

    DtDvbC2DemodL1Part2Data();
    
    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbC2StreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for DVB-C2 PLP-stream selection
//
struct DtDvbC2StreamSelPars
{
    int  m_DSliceId;                // ID of the data slice or DTAPI_DVBC2_DSLICE_ID_AUTO
    int  m_PlpId;                   // ID of the data PLP or DTAPI_DVBC2_PLP_ID_xxx
    int  m_CommonPlpId;             // ID of the common PLP or DTAPI_DVBC2_PLP_ID_xxx

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DVB-S2 Parameters +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

#define DTAPI_DVBS2_NUM_PLP_MAX     255          // Maximum number of PLPs

// m_Issy - PLP ISSY
#define DTAPI_DVBS2_ISSY_NONE       0            // No ISSY field is used
#define DTAPI_DVBS2_ISSY_SHORT      1            // 2 byte ISSY field is used
#define DTAPI_DVBS2_ISSY_LONG       2            // 3 byte ISSY field is used

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDvbS2ModStatus -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
struct DtDvbS2ModStatus
{
    int  m_MplpModFlags;            // Multi PLP modulator flags
    __int64  m_DjbOverflows;        // Count number of DJB overflows. If it happens,
                                    // issy output delay must be decreased or "issy bufs"
                                    // increased.
    __int64  m_DjbUnderflows;       // Count number of DJB underflows. If it happens,
                                    // issy output delay must be increased.
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDvbS2FecFrameHeader -.-.-.-.-.-.-.-.-.-.-.-.-.-.-
struct DtDvbS2FecFrameHeader
{
    int  m_Modulation;              // PLP Modulation. See DTAPI_MOD_DVBS2_*
    int  m_CodeRate;                // PLP Code rate. See DTAPI_MOD_x
    int  m_FecFrameSize;            // Fec frame size. See DTAPI_MOD_S2_*FRM
    bool  m_HasPilots;              // Enable pilots
    int  m_FecFrameCount;           // Number of successive FEC frames using these
                                    // parameters, 0 means infinite.
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDvbS2ModCod -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
struct DtDvbS2ModCod
{
    int  m_ModType;                 // Modulation type, e.g. DTAPI_MOD_DVBS_QPSK
    int  m_CodeRate;                // Code rate, e.g. DTAPI_MOD_1_2
    // Constructor
    DtDvbS2ModCod();
    DtDvbS2ModCod(int ModType, int CodeRate);
    bool operator < (const DtDvbS2ModCod& ModCod) const;
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDvbS2PlpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
struct DtDvbS2PlpPars
{
    bool  m_Hem;                    // High Efficiency Mode
    bool  m_Npd;                    // Null Packet Deletion
    int  m_Issy;                    // Issy mode. See DTAPI_DVBS2_ISSY_x
    int  m_IssyBufs;                // Issy BUFS
    int  m_IssyOutputDelay;         // Delay (in T units) between the incoming data and
                                    // the output TS in the receiver model. This value 
                                    // determines the minimum and maximum dejitter buffer
                                    // usage and is used to compute the ISSY BUFSTAT field
    int  m_TsRate;                  // Ts rate
    int  m_Ccm;                     // ACM/CMM bit in the BBframe header 0 or 1
    int  m_Id;                      // PLP ID. 0..255

    // One or more fec frame headers. If there is only 1 the m_FecFrameCount member
    // is ignored. Otherwise that specifies the number of frames to generate with those
    // parameters. When that number of frames are generated, the next set of parameters
    // is taken. After the last DtDvbS2FecFrameHeader the first one is used again.
    std::vector<DtDvbS2FecFrameHeader> m_AcmHeaders;
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- struct DtDvbS2Pars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
struct DtDvbS2Pars
{
    DtVirtualOutPars  m_VirtOutput; // Virtual-output parameters (Optional) 
    bool  m_L3Output;               // Set to true to enable L3 output
    int  m_SymRate;                 // Symbol rate
    int  m_RollOff;                 // Roll-off DTAPI_MOD_ROLLOFF_xxx
    
    // Data per PLP
    std::vector<DtDvbS2PlpPars>  m_Plps;
    // PLP input
    int  m_NumPlpInputs;            // Number of PLPs
    DtPlpInpPars  m_PlpInputs[DTAPI_DVBS2_NUM_PLP_MAX];  // PLP inputs (Optional)

    DtDvbS2Pars();
    DTAPI_RESULT  CheckValidity();
};

//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ DVB-T2 Parameters +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

// Maxima
#define DTAPI_DVBT2_NUM_PLP_MAX     255          // Maximum number of PLPs
#define DTAPI_DVBT2_NUM_RF_MAX      7            // Maximum number of RF output signals

// PLP IDs
#define DTAPI_DVBT2_PLP_ID_NONE     -1           // No PLP selected
#define DTAPI_DVBT2_PLP_ID_AUTO     -2           // Automatic PLP selection

// m_Issy
#define DTAPI_DVBT2_ISSY_NONE       0            // No ISSY field is used
#define DTAPI_DVBT2_ISSY_SHORT      1            // 2-byte ISSY field is used
#define DTAPI_DVBT2_ISSY_LONG       2            // 3-byte ISSY field is used

// m_Bandwidth
#define DTAPI_DVBT2_1_7MHZ          0            // 1.7 MHz
#define DTAPI_DVBT2_5MHZ            1            // 5 MHz
#define DTAPI_DVBT2_6MHZ            2            // 6 MHz
#define DTAPI_DVBT2_7MHZ            3            // 7 MHz
#define DTAPI_DVBT2_8MHZ            4            // 8 MHz
#define DTAPI_DVBT2_10MHZ           5            // 10 MHz
#define DTAPI_DVBT2_BW_UNK          -1           // Unknown bandwith
#define DTAPI_DVBT2MI_BW_MSK        0xF          // Mask for T2MI ParXtra2
#define DTAPI_DVBT2MI_BW_UNK        0xF          //Val in ParXtra2 if not set, map to 8MHz

// m_FftMode
// Warning: the codes are different from the corresponding L1 field
#define DTAPI_DVBT2_FFT_1K          0            // 1K FFT
#define DTAPI_DVBT2_FFT_2K          1            // 2K FFT
#define DTAPI_DVBT2_FFT_4K          2            // 4K FFT
#define DTAPI_DVBT2_FFT_8K          3            // 8K FFT
#define DTAPI_DVBT2_FFT_16K         4            // 16K FFT
#define DTAPI_DVBT2_FFT_32K         5            // 32K FFT
#define DTAPI_DVBT2_FFT_UNK         -1           // Unknown FFT mode

// m_Miso
#define DTAPI_DVBT2_MISO_OFF        0            // No MISO
#define DTAPI_DVBT2_MISO_TX1        1            // TX1 only
#define DTAPI_DVBT2_MISO_TX2        2            // TX2 only
#define DTAPI_DVBT2_MISO_TX1TX2     3            // TX1+TX2 Legacy
#define DTAPI_DVBT2_MISO_SUM        3            // TX1+TX2
#define DTAPI_DVBT2_MISO_BOTH       4            // TX1 and TX2

// m_Guard - Guard interval
// Warning: the codes are different from the corresponding L1 field
#define DTAPI_DVBT2_GI_1_128        0            // 1/128
#define DTAPI_DVBT2_GI_1_32         1            // 1/32
#define DTAPI_DVBT2_GI_1_16         2            // 1/16
#define DTAPI_DVBT2_GI_19_256       3            // 19/256
#define DTAPI_DVBT2_GI_1_8          4            // 1/8
#define DTAPI_DVBT2_GI_19_128       5            // 19/128
#define DTAPI_DVBT2_GI_1_4          6            // 1/4
#define DTAPI_DVBT2_GI_UNK          -1           // Unknown guard interval

// m_Papr - PAPR - Peak to Average Power Reduction
#define DTAPI_DVBT2_PAPR_NONE       0
#define DTAPI_DVBT2_PAPR_ACE        1            // ACE - Active Constellation Extension
#define DTAPI_DVBT2_PAPR_TR         2            // TR - PAPR using reserved carriers
#define DTAPI_DVBT2_PAPR_ACE_TR     3            // ACE and TR

// m_BwtExt - Bandwidth extension
#define DTAPI_DVBT2_BWTEXT_OFF      false        // No bandwidth extension
#define DTAPI_DVBT2_BWTEXT_ON       true         // Bandwidth extension on

// m_PilotPattern
// Warning: the codes are different from the corresponding L1 field
#define DTAPI_DVBT2_PP_1            1            // PP1
#define DTAPI_DVBT2_PP_2            2            // PP2
#define DTAPI_DVBT2_PP_3            3            // PP3
#define DTAPI_DVBT2_PP_4            4            // PP4
#define DTAPI_DVBT2_PP_5            5            // PP5
#define DTAPI_DVBT2_PP_6            6            // PP6
#define DTAPI_DVBT2_PP_7            7            // PP7
#define DTAPI_DVBT2_PP_8            8            // PP8

// m_CodeRate - Code rate
#define DTAPI_DVBT2_COD_1_2         0            // 1/2
#define DTAPI_DVBT2_COD_3_5         1            // 3/5
#define DTAPI_DVBT2_COD_2_3         2            // 2/3
#define DTAPI_DVBT2_COD_3_4         3            // 3/4
#define DTAPI_DVBT2_COD_4_5         4            // 4/5 not for T2 lite
#define DTAPI_DVBT2_COD_5_6         5            // 5/6 not for T2 lite
#define DTAPI_DVBT2_COD_1_3         6            // 1/3 only for T2 lite
#define DTAPI_DVBT2_COD_2_5         7            // 2/5 only for T2 lite

// m_FefSignal - Type of signal generated during the FEF period
#define DTAPI_DVBT2_FEF_ZERO        0            // Use zero I/Q samples during FEF
#define DTAPI_DVBT2_FEF_1K_OFDM     1            // 1K OFDM symbols with 852 active
                                                 // carriers containing BPSK symbols
                                                 // (same PRBS as the T2 dummy cells,
                                                 // not reset between symbols)
#define DTAPI_DVBT2_FEF_1K_OFDM_384 2            // 1K OFDM symbols with 384 active
                                                 //  carriers containing BPSK symbols

// m_PlpConstel and m_L1Constel - Modulation constellation
#define DTAPI_DVBT2_BPSK            0            // BPSK
#define DTAPI_DVBT2_QPSK            1            // QPSK
#define DTAPI_DVBT2_QAM16           2            // 16-QAM
#define DTAPI_DVBT2_QAM64           3            // 64-QAM
#define DTAPI_DVBT2_QAM256          4            // 256-QAM

// m_Type - PLP type
#define DTAPI_DVBT2_PLP_TYPE_COMM   0            // Common PLP
#define DTAPI_DVBT2_PLP_TYPE_1      1            // PLP type 1
#define DTAPI_DVBT2_PLP_TYPE_2      2            // PLP type 2

// m_FecType - PLP FEC type
#define DTAPI_DVBT2_LDPC_16K        0            // 16K LDPC
#define DTAPI_DVBT2_LDPC_64K        1            // 64K LDPC

// m_TimeIlType - Time interleaving type
#define DTAPI_DVBT2_IL_ONETOONE     0            // Interleaving frame in one T2 frame
#define DTAPI_DVBT2_IL_MULTI        1            // Interleaving frame in multiple frames

// m_TimeStamping - Type of timestamps in T2MI
#define DTAPI_DVBT2MI_TIMESTAMP_NULL 0           // No timestamping
#define DTAPI_DVBT2MI_TIMESTAMP_REL 1            // Relative timestamps. Use m_Subseconds
#define DTAPI_DVBT2MI_TIMESTAMP_ABS 2            // Absolute timestamps. Use m_T2miUtco,
                                                 // m_SecSince2000, m_Subseconds,

// m_T2Version - DVB-T2 specification version
#define DTAPI_DVBT2_VERSION_1_1_1   0            // DVB-T2 version 1.1.1
#define DTAPI_DVBT2_VERSION_1_2_1   1            // DVB-T2 version 1.2.1
#define DTAPI_DVBT2_VERSION_1_3_1   2            // DVB-T2 version 1.3.1

// m_T2Profile - DVB-T2 profile
#define DTAPI_DVBT2_PROFILE_BASE    0         
#define DTAPI_DVBT2_PROFILE_LITE    1            // Requires DVB-T2 version 1.3.1

// m_BiasBalancing
#define DTAPI_DVBT2_BIAS_BAL_OFF    0            // No L1 bias compensation
#define DTAPI_DVBT2_BIAS_BAL_ON     1            // Modify L1 reserved fields and L1 ext.
                                                 // field padding to compensate L1 bias
// m_GseLabelType - DVB-T2 GSE Label size
#define DTAPI_DVBT2_GSE_LABEL_6BYTE 0           // 6 Byte GSE label
#define DTAPI_DVBT2_GSE_LABEL_3BYTE 1           // 3 Byte GSE label
#define DTAPI_DVBT2_GSE_LABEL_NONE  2           // No GSE label

#define DTAPI_TXSIG_FEF_LEN_MIN  162212          // Min. FEF length for FEF TX sgnalling

// DVB-T2 test point enum
enum {
    DTAPI_DVBT2_TP00,
    DTAPI_DVBT2_TP01,
    DTAPI_DVBT2_TP03,
    DTAPI_DVBT2_TP04,
    DTAPI_DVBT2_TP06,
    DTAPI_DVBT2_TP08,
    DTAPI_DVBT2_TP09,
    DTAPI_DVBT2_TP11,
    DTAPI_DVBT2_TP12,
    DTAPI_DVBT2_TP15,
    DTAPI_DVBT2_TP16,
    DTAPI_DVBT2_TP19,               // Only usable if CFLOAT32 output format is selected
    DTAPI_DVBT2_TP20,
    DTAPI_DVBT2_TP21,
    DTAPI_DVBT2_TP22,
    DTAPI_DVBT2_TP23,
    DTAPI_DVBT2_TP24,
    DTAPI_DVBT2_TP25,
    DTAPI_DVBT2_TP26,
    DTAPI_DVBT2_TP27,
    DTAPI_DVBT2_TP28,
    DTAPI_DVBT2_TP29,
    DTAPI_DVBT2_TP30,
    DTAPI_DVBT2_TP32,
    DTAPI_DVBT2_TP33,               // T2MI output
    DTAPI_DVBT2_TP34,               // T2MI output

    // Receiver Buffer Model
    DTAPI_DVBT2_TP50,               // TDI size 
    DTAPI_DVBT2_TP51,               // TDI write index, TDI read available
    DTAPI_DVBT2_TP53,               // DJB size 
    DTAPI_DVBT2_TP_COUNT,           // Number of test points
};

extern const int  DTAPI_DVBT2_TESTPOINTS[DTAPI_DVBT2_TP_COUNT];

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2AuxPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying the parameters of AUX streams
//
struct DtDvbT2AuxPars
{
    int  m_NumDummyStreams;         // Number of dummy AUX streams

public:
    void  Init(void);
    bool  IsEqual(DtDvbT2AuxPars& AuxPars);
    bool  operator == (DtDvbT2AuxPars& AuxPars);
    bool  operator != (DtDvbT2AuxPars& AuxPars);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2MiPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for specifying the parametes for T2MI output streams
//
struct DtDvbT2MiPars
{
    bool  m_Enabled;                // Enable T2MI output. If enabled, a T2MI
                                    // Transport Stream is generated and output
    int  m_Pid;                     // T2MI data PID
    int  m_StreamId;                // stream ID for the first T2MI stream 
    int  m_Pid2;                    // Second T2MI data PID
    int  m_StreamId2;               // stream ID for the second T2MI stream
    int  m_PcrPid;                  // PCR PID. If -1, no PCR is included otherwise
                                    // PCRs are inserted on the specified PID
    int  m_PmtPid;                  // PMT PID. If -1, no PMT-table and no PAT-table
                                    // are included otherwise a PMT-table is inserted 
                                    // on the specified PID
    int  m_TsRate;                  // Rate in bps for the T2MI output
    int  m_TimeStamping;            // T2MI timestamps: None, Absolute or Releative
                                    // See DVBT2MI_TIMESTAMP_x
    __int64  m_SecSince2000;        // First T2MI output timestamp value. Next values
                                    // are computed
    int  m_Subseconds;              // The number of subseconds. See T2MI spec table 4 
    int  m_T2miUtco;                // Offset in seconds between UTC and m_SecSince2000.
                                    // As of February 2009 the value shall be 2 and 
                                    // shall change as a result of each new leap second.
    bool  m_EncodeFef;              // If true, outputs a FEF part composite packet 
                                    // with the required subpart. Otherwise, only 
                                    // outputs a FEF part NULL packet when FEF is
                                    // enabled.
    bool  m_SyncWithExtClock;       // Undocumented. For internal use only.
public:
    void  Init(void);
    bool  IsEqual(DtDvbT2MiPars& T2MiPars);
    bool  operator == (DtDvbT2MiPars& T2MiPars);
    bool  operator != (DtDvbT2MiPars& T2MiPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2ModStatus -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure for retrieving the MPLP modulator status
//
struct DtDvbT2ModStatus
{
    int  m_MplpModFlags;            // Multi PLP modulator flags
    // General MPLP status info
    __int64  m_PlpNumBlocksOverflows;
                                    // Counts the FEC frames for which the requested
                                    // number of PLP blocks is bigger than NumBlocks
                                    // (the receiver will get an invalid stream)
    __int64  m_BitrateOverflows;    // Counts the frames in which too many bits were
                                    // allocated (the receiver will get an invalid stream)
    __int64  m_TtoErrorCount;       // Number of times the generated TTO value was
                                    // invalid (typically from a too small T_design)
    
    // T2MI Specific status info
    __int64  m_T2MiOutputRateOverFlows;
                                    // Number of bit rate overflows(i.e. the T2MI output
                                    // rate must be increased for reliable operation)
    int  m_T2MiOutputRate;          // Current T2MI rate excluding null packets(in bps)
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2PaprPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying and enabling the PAPR reduction parameters
//
struct DtDvbT2PaprPars
{
    bool  m_AceEnabled;             // ACE enabled
    double  m_AceVclip;             // ACE clipping threshold 1..4.32 (Volt)
    double  m_AceGain;              // ACE gain 0..31 (steps of 1)
    double  m_AceLimit;             // ACE limit 0.7..1.4 (steps of 0.1) 
    int  m_AceInterpFactor;         // ACE interpolation factor 1..4
                                    // Note: PAPR ACE processing time is proportional
                                    // to this parameter (1 recommended for realtime)
    int  m_AcePlpIndex;             // PLP used for the PAPR ACE
    bool  m_TrEnabled;              // TR enabled
    bool  m_TrP2Only;               // PAPR TR is only applied on the P2 symbol
    double  m_TrVclip;              // TR clipping threshold 1..4.32 (Volt)
    int  m_TrMaxIter;               // TR maximum number of iterations. Must be >= 1
                                    // Note: PAPR TR processing time is proportional
                                    // to this parameter
    int  m_L1ExtLength;             // L1 extension field length 0..65535
    bool  m_L1AceEnabled;           // L1 ACE enabled
    double  m_L1AceCMax;            // L1 ACE maximum constellation extension value
    bool  m_L1Scrambling;           // L1 post scrabling (requires T2-version 1.3.1)

    // Parameters below only apply if DVB-T2 V1.2.1 is selected
    int  m_NumBiasBalCells;         // Dummy cells added to reduce the P2 PAPR
                                    // 0..BiasBalancingCellsMax
    int  m_BiasBalancing;           // Modify the L1 reserved fields and 
                                    // L1 ext padding to compensate the L1 bias. 
                                    // See DTAPI_DVBT2_BIAS_x
    int  m_TrAlgorithm;             // Undocumented. Must be 1 (default)

public:
    void  Init(void);
    bool  IsEqual(DtDvbT2PaprPars& PaprPars);
    bool  operator == (DtDvbT2PaprPars& PaprPars);
    bool  operator != (DtDvbT2PaprPars& PaprPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2ParamInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// DVB-T2 parameter info
//
struct DtDvbT2ParamInfo 
{
    int  m_TotalCellsPerFrame;      // Total number of cells per frame
    int  m_L1CellsPerFrame;         // Total #cells per frame used for L1 signaling
                                    // Overhead: m_L1CellsPerFrame/m_TotalCellsPerFrame
    int  m_AuxCellsPerFrame;        // Total number of auxiliary stream cells per frame 
                                    // (currently only used for TX signalling if enabled)
    int  m_BiasBalCellsPerFrame;    // Total number of L1 bias balancing cells per frame
    int  m_BiasBalCellsMax;         // Maximum number of L1 bias balancing cells per P2
    int  m_DummyCellsPerFrame;      // Total number of cells lost per frame. Dummy cells
                                    // overhead: m_DummyCellsPerFrame/m_TotalCellsPerFrame
                                    // It is computed in case no NDP is used for frame 0.
    int  m_SamplesPerFrame;         // Total number of samples per frame
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2PlpPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying the parameters of a PLP
//
struct DtDvbT2PlpPars
{
    // Mode adaptation layer: TS input
    bool  m_Hem;                    // High Efficiency Mode: yes/no
    bool  m_Npd;                    // Null Packet Deletion: yes/no
    int  m_Issy;                    // ISSY mode. See DTAPI_DVBT2_ISSY_XXX
    int  m_IssyBufs;                // ISSY BUFS
    int  m_IssyTDesign;             // T-design value for TTO generation. 
                                    // Use 0 to have the modulator choose the value.
                                    // T-design is defined as the delay (in samples)
                                    // between the start of the first T2 frame in 
                                    // which the PLP is mapped (m_FirstFrameIdx) and
                                    // the first output bit of the transport stream.
    int  m_CompensatingDelay;       // Additional delay (in samples) before the TS 
                                    // data is sent. Use -1 to have the modulator 
                                    // choose the value
    int  m_TsRate;                  // If 0 the rate is computed from the PLP 
                                    // parameters. Only possible if no NPD is used.
    // Mode adaptation layer: GSE input
    int  m_GseLabelType;            // GSE-label type. See DTAPI_DVBT2_GSE_LABEL_XXX

    // DVB-T2 L1 parameters
    int  m_Id;                      // PLP ID: 0..255
    int  m_GroupId;                 // PLP group ID: 0..255
    int  m_Type;                    // PLP type: DTAPI_DVBT2_PLP_TYPE_XXX
    int  m_PayloadType;             // PLP payload type: DTAPI_DVBT2_PAYLOAD_XXX
    int  m_CodeRate;                // PLP code rate: DTAPI_DVBT2_COD_XXX
    int  m_Modulation;              // PLP modulation: DTAPI_DVBT2_BPSK/...
    bool  m_Rotation;               // Constellation rotation: yes/no
    int  m_FecType;                 // FEC Type. 0=16K, 1=64K; Must be 16K for T2 lite
    int  m_FrameInterval;           // T2-frame interval for this PLP: 1..255
    int  m_FirstFrameIdx;           // First frame index: 0..m_FrameInterval-1
    int  m_TimeIlLength;            // Time interleaving length: 0..255
    int  m_TimeIlType;              // Time interleaving type: DTAPI_DVBT2_IL_XXX
    bool  m_InBandAFlag;            // In band A signaling information: yes/no
    bool  m_InBandBFlag;            // In band B Signaling information: yes/no
                                    // Only useful if DVB-T2 V1.2.1 is selected
    int  m_NumBlocks;               // Maximum number of FEC blocks contained in
                                    // one interleaving frame

    // IDs of the other PLPs in the in-band signaling
    int  m_NumOtherPlpInBand;       // Number of other PLPs in m_OtherPlpInBand
    int  m_OtherPlpInBand[DTAPI_DVBT2_NUM_PLP_MAX-1];

    // The parameters below are only meaningful for type 1 PLPs in TFS case
    bool  m_FfFlag;                 // FF flag
    int  m_FirstRfIdx;              // First TFS RF channel: 0..NumRf-1

public:
    void  Init(int PlpId = 0);
    bool  IsEqual(DtDvbT2PlpPars& PlpPars);
    bool  operator == (DtDvbT2PlpPars& PlpPars);
    bool  operator != (DtDvbT2PlpPars& PlpPars);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- RBM events -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// RBM events all are errors except plot
//
enum DtDvbT2RbmEventType 
{
    // Plot event (only generated if m_PlotEnabled is true)
    DTAPI_DVBT2_RBM_EVENT_PLOT,
    // Dejitter buffer underflow
    DTAPI_DVBT2_RBM_EVENT_DJB_UNDERFLOW,
    // BUFS=[m_Bufs] gives too small dejitter buffer
    DTAPI_DVBT2_RBM_EVENT_BUFS_TOO_SMALL,
    // TTO=[m_Tto] gives time in the past
    DTAPI_DVBT2_RBM_EVENT_TTO_IN_THE_PAST,
    // DJB overflow (should never happen except if the model is buggy) 
    DTAPI_DVBT2_RBM_EVENT_DJB_OVERFLOW,
    // BBFrame parser: CRC8 error in header (val=[m_Val]) 
    DTAPI_DVBT2_RBM_EVENT_CRC8_ERROR_HEADER,
    // BBFrame parser: DFL too large 
    DTAPI_DVBT2_RBM_EVENT_DFL_TOO_LARGE,
    // BBFrame parser: SYNCD too large (SYNCD=[m_SyncD] DFL=[m_Dfl]) 
    DTAPI_DVBT2_RBM_EVENT_SYNCD_TOO_LARGE,
    // BBFrame parser: invalid UPL 
    DTAPI_DVBT2_RBM_EVENT_INVALID_UPL,
    // BBFrame parser: invalid syncd (syncd=[m_SyncD] left=[m_Left]) 
    DTAPI_DVBT2_RBM_EVENT_INVALID_SYNCD,
    // TDI overflow: write pointer ([m_TdiWriteIndex]) ahead of 
    // read pointer ([m_TdiReadIndex]) 
    DTAPI_DVBT2_RBM_EVENT_TDI_OVERFLOW,
    // TDI overflow: too many TI blocks queued  
    DTAPI_DVBT2_RBM_EVENT_TOO_MANY_TI_BLOCKS,
    // plp_start value gives overlap between PLP id=[m_PlpId1] and id=[m_PlpId2] 
    DTAPI_DVBT2_RBM_EVENT_INVALID_PLP_START,
    // Frequency/L1 deinterleaver overflow 
    DTAPI_DVBT2_RBM_EVENT_FDI_OVERFLOW,
    // Not enough ISCR data to estimate the TS rate 
    DTAPI_DVBT2_RBM_EVENT_NO_TS_RATE,
    // ISCR error (delta=[m_Delta]) 
    DTAPI_DVBT2_RBM_EVENT_ISCR_ERROR,
    // BUFS not constant (current=[m_CurBufs] new=[m_NewBufs]) 
    DTAPI_DVBT2_RBM_EVENT_BUFS_NOT_CONSTANT,
    // ISSYI field cannot change its value 
    DTAPI_DVBT2_RBM_EVENT_ISSYI_NOT_CONSTANT,
    // HEM field cannot change its value 
    DTAPI_DVBT2_RBM_EVENT_HEM_NOT_CONSTANT,
    // plp_num_blocks for this interleaving frame is too small (plp_num_blocks=%d) 
    // At least 3 FEC block required in interleaving frame with HEM=1,
    // at least 1 if HEM=0. 
    DTAPI_DVBT2_RBM_EVENT_PLP_NUM_BLOCKS_TOO_SMALL,
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2RbmEvent -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// RBM event parameters
//
struct DtDvbT2RbmEvent
{
    int  m_DataPlpId;               // Data plp ID identifiying the stream
    int  m_DataPlpIndex;            // Data plp index
    double  m_Time;                 // Time in T units
    int  m_IsCommonPlp;             // 1 = common PLP, 0 = data PLP,
                                    // -1 = the event does not refer to a particular PLP
    DtDvbT2RbmEventType m_EventType;
    // Additional parameters
    union {
        // DTAPI_DVBT2_RBM_EVENT_PLOT parameters
        struct {
            int  m_TdiWriteIndex;   // TDI write index
            int  m_TdiReadIndex;    // TDI read index
            int  m_TdiReadAvailable;// Available cells in the TDI read buffer
            int  m_DjbSize;         // Dejitter buffer size in bits
        } Plot;

        // DTAPI_DVBT2_RBM_EVENT_BUFS_TOO_SMALL parameters
        struct {
            int  m_Bufs;            // BUFS value
        } BufsTooSmall;

        // DTAPI_DVBT2_RBM_EVENT_TTO_IN_THE_PAST parameters
        struct {
            int  m_Tto;             // TTO value
        } TtoInThePast;

        // DTAPI_DVBT2_RBM_EVENT_DJB_OVERFLOW paraneters
        struct {
            int  m_DjbSize;         // Dejitter buffer size in bits
            int  m_DjbMaxSize;
        } DjbOverflow;

        // DTAPI_DVBT2_RBM_EVENT_CRC8_ERROR_HEADER parameters
        struct {
            int  m_Val;             // CRC8 value
        } Crc8ErrorHeader;

        // DTAPI_DVBT2_RBM_EVENT_DFL_TOO_LARGE parameters
        struct {
            int  m_SyncD;           // SYNCD
            int  m_Dfl;             // DFL
        } SyncDTooLarge;
        
        // DTAPI_DVBT2_RBM_EVENT_INVALID_SYNCD parameters
        struct {
            int  m_Syncd;           // SYNCD
            int  m_Left;            // Left
        } InvalidSyncD;
        
        // DTAPI_DVBT2_RBM_EVENT_TDI_OVERFLOW parameters
        struct {
            int  m_TdiWriteIndex;   // TDI write index
            int  m_TdiReadIndex;    // TDI read index
        } TdiOverflow;

        // DTAPI_DVBT2_RBM_EVENT_INVALID_PLP_START parameters
        struct {
            int  m_PlpId1;          // IDs of overlapping PLPs
            int  m_PlpId2;
        } InvalidPlpStart;

        // DTAPI_DVBT2_RBM_EVENT_ISCR_ERROR parameters
        struct {
            int  m_Delta;           // Delta time in T units 
        } IscrError;
        
        // DTAPI_DVBT2_RBM_EVENT_BUFS_NOT_CONSTANT parameters
        struct {
            int  m_CurBufs;         // Different BUFS values
            int  m_newBufs;
        } BufsNotConstant;
        
        // DTAPI_DVBT2_RBM_EVENT_PLP_NUM_BLOCKS_TOO_SMALL parameters
        struct {
            int  m_PlpNumBlocks;    // Number of blocks
        } PlpNumBlocksTooSmall;
    } u;
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2RbmValidation -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for enabling the RBM validation and specifying a callback function
//
struct DtDvbT2RbmValidation
{
public:
    bool  m_Enabled;                // Enable RBM validation
    bool  m_PlotEnabled;            // Enable RBM plotting events
    int  m_PlotPeriod;              // Plot period
    void*  m_pCallbackOpaque;       // RBM event callback function and environment
    void  (*m_pCallbackFunc)(void *pOpaque, const DtDvbT2RbmEvent* pRbmEvent);

public:
    void  Init(void);
    bool  IsEqual(DtDvbT2RbmValidation& RbmPars);
    bool  operator == (DtDvbT2RbmValidation& RbmPars);
    bool  operator != (DtDvbT2RbmValidation& RbmPars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2TxSigPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying and enabling the Transmitter Signature
//
struct DtDvbT2TxSigPars
{
    // TX Signature through Auxiliary Streams
    // The total number of possible TX IDs are M=3*(P+1)
    // The number of cells used per transmitter is N=2^Q 
    // The number of of T2 frames per TX SIG frame is L=R+1 
    bool  m_TxSigAuxEnabled;        // Enabled
    int  m_TxSigAuxId;              // Transmitter ID. 0..3071
    int  m_TxSigAuxP;               // P 0..1023. The total number of possible 
                                    // TX IDs is M=3*(P+1). Hence M <= 3072
    int  m_TxSigAuxQ;               // Q 0..15. The number of cells used per     
                                    // transmitter is N=2^Q
    int  m_TxSigAuxR;               // R 0..255. The number of T2 frames
                                    // per TX SIG frame is L=R+1

    // TX Signature through FEF. 
    // To use this FEF generation must be enabled and  the FEF length 
    // must be >= DTAPI_TXSIG_FEF_LEN_MIN
    bool  m_TxSigFefEnabled;        // Enabled
    int  m_TxSigFefId1;             // TX ID for 1st signature period. 0..7
    int  m_TxSigFefId2;             // TX ID for 2nd signature period. 0..7

public:
    void  Init(void);
    bool  IsEqual(DtDvbT2TxSigPars& TxSigPars);
    bool  operator == (DtDvbT2TxSigPars& TxSigPars);
    bool  operator != (DtDvbT2TxSigPars& TxSigPars);
};

// Compare modes
#define DTAPI_DVBT2_COMPA_ALL       0
#define DTAPI_DVBT2_COMPA_ESSENTIAL 1

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2ComponentPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class for specifying the DVB-T2 component modulation parameters
//
class DtDvbT2ComponentPars
{
public:
    // General T2 parameters
    int  m_T2Version;               // DVB-T2 spec. version. See DVBT2_VERSION_x
    int  m_T2Profile;               // DVB-T2 profile. See DVBT2_PROFILE_x
    bool  m_T2BaseLite;             // Indicates whether T2 lite is used in a base
                                    // profile stream
    int  m_Bandwidth;               // Bandwidth: DVBT2_BW_XXX
    int  m_FftMode;                 // FFT-mode: DVBT2_FFT_XXX
    int  m_Miso;                    // MISO. See DVBT2_MISO_x
    int  m_GuardInterval;           // Guard interval. See DVBT2_GI_x
    int  m_Papr;                    // PAPR. See DVBT2_PAPR_x
    bool  m_BwtExt;                 // Bandwidth extention
    int  m_PilotPattern;            // Pilot pattern. Pattern 1 .. 8.
    int  m_L1Modulation;            // L1 modulation. See DVBT2_BPSK/... 
    int  m_CellId;                  // Cell ID.  0..0xFFFF.
    int  m_NetworkId;               // Network ID.  0..0xFFFF.
    int  m_T2SystemId;              // T2 System ID.  0..0xFFFF.
    bool  m_L1Repetition;           // L1 repetition

    // T2-Frame related parameters
    int  m_NumT2Frames;             // #T2-frames per superframe. 1..255. 
    int  m_NumDataSyms;             // #Data symbols
    int  m_NumSubslices;            // #Subslices per T2-frame (for type2 PLPs)

    // Component start time
    int  m_ComponentStartTime;      // Offset in T unit at which the T2 component 
                                    // begins to be modulated. (0 in the first component)
    // FEF parameters
    bool  m_FefEnable;              // FEF enable
    int  m_FefType;                 // FEF type. 0..15
    int  m_FefS1;                   // FEF S1. 2..7
    int  m_FefS2;                   // FEF S2. 1, 3, 5, 7, 9 ,11, 13 or 15
    int  m_FefSignal;               // Selects the type of signal generated during 
                                    // the FEF period (see DTAPI_DVBT2_FEF_x)
    int  m_FefLength;               // FEF Length in number of samples.
    int  m_FefInterval;             // FEF Interval.  
                                    //  Requires: (m_NumT2Frames % m_FefInterval) == 0
    // RF channels for TFS
    int  m_NumRfChans;              // Number of RF channels 1..7
    int  m_RfChanFreqs[DTAPI_DVBT2_NUM_RF_MAX];
                                    // Channel frequencis
    int  m_StartRfIdx;              // First used RF channel

    // PLPs
    int  m_NumPlps;                 // Number of PLPs
    DtDvbT2PlpPars  m_Plps[DTAPI_DVBT2_NUM_PLP_MAX];
                                    // PLP parameters
    DtPlpInpPars  m_PlpInputs[DTAPI_DVBT2_NUM_PLP_MAX];  
                                    // PLP inputs (Optional)
    // Optional
    DtDvbT2AuxPars  m_Aux;          // AUX Streams (Optional)
    DtDvbT2PaprPars  m_PaprPars;    // PAPR Params (Optional)
    DtDvbT2TxSigPars  m_TxSignature;// TX-Signature (Optional)
    DtDvbT2RbmValidation  m_RbmValidation;
                                    // RBM validation (Optional)
    DtTestPointOutPars  m_TpOutput; // Test point data output parameters (optional)

    int  m_L1ChangeCounter;         // Undocumented. For internal use only.

public:
    virtual void  Init(void);
    virtual bool  IsEqual(DtDvbT2ComponentPars&, int CompareMode=DTAPI_DVBT2_COMPA_ALL);
    virtual bool  operator == (DtDvbT2ComponentPars& T2Pars);
    virtual bool  operator != (DtDvbT2ComponentPars& T2Pars);
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2Pars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Class for specifying the DVB-T2 modulation parameters.
//
class DtDvbT2Pars : public DtDvbT2ComponentPars
{
public:
    // Optional
    DtVirtualOutPars  m_VirtOutput; // Virtual-output parameters (Optional) 
    DtDvbT2MiPars  m_T2Mi;          // T2MI output (Optional)
    int  m_NumFefComponents;        // Number of other T2 stream transmitted in the 
                                    // FEF part of the first component. The parameters 
                                    // come from 'm_FefComponent'.

    // FEF components. Currently maximum 1 other component
    DtDvbT2ComponentPars  m_FefComponent[1];

public:
      // Constructor
    DtDvbT2Pars() { Init(); }

    // Methods
    virtual void  Init(void);
    virtual DTAPI_RESULT  CheckValidity(void);
    virtual DTAPI_RESULT  ComputeTDesign();
    virtual DTAPI_RESULT  GetParamInfo(DtDvbT2ParamInfo& T2Info);
    virtual DTAPI_RESULT  GetParamInfo(DtDvbT2ParamInfo& T2Info1, 
                                                               DtDvbT2ParamInfo& T2Info2);
    
    // Only usefull for single PLP
    DTAPI_RESULT  OptimisePlpNumBlocks(DtDvbT2ParamInfo&, int&);
    DTAPI_RESULT  OptimisePlpNumBlocks(DtDvbT2ParamInfo&, int&, int&);

    // Helper function to determine T2MI ts rate.
    static DTAPI_RESULT  RetrieveT2miTsRateFromTs(char* pBuffer, int NumBytes,
                                                            int  Bandwidth, int&  TsRate);

    // Operators
    virtual bool  operator == (DtDvbT2Pars& T2Pars);
    virtual bool  operator != (DtDvbT2Pars& T2Pars);
    virtual bool  IsEqual(DtDvbT2Pars& T2Pars, int CompareMode=DTAPI_DVBT2_COMPA_ALL);
};

//=+=+=+=+=+=+=+=+=+=+=+=+=+ DVB-T2 Demodulation layer 1 data  +=+=+=+=+=+=+=+=+=+=+=+=+=+

// DtDvbT2DemodL1PostPlp::m_PayloadType - The PLP payload type
#define DTAPI_DVBT2_PAYLOAD_GFPS    0            // Generic Fixed-length Packetized Stream
#define DTAPI_DVBT2_PAYLOAD_GCS     1            // Generic Continuous Stream
#define DTAPI_DVBT2_PAYLOAD_GSE     2            // Generic Stream Encapsulation
#define DTAPI_DVBT2_PAYLOAD_TS      3            // Transport Stream

// DtDvbT2DemodL1Pre::m_Type - The current T2 super-frame stream type
#define DTAPI_DVBT2_TYPE_TS         0            // Transport Stream (TS) only
#define DTAPI_DVBT2_TYPE_GS         1            // Generic Stream (GS) only
#define DTAPI_DVBT2_TYPE_TS_GS      2            // Mixed TS and GS

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2DemodAuxPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Struct for storing the Auxiliary stream information from Layer 1 Post 
//
struct DtDvbT2DemodAuxPars
{
    int  m_AuxStreamType;           // Auxiliary stream type
    int  m_AuxPrivateConf;          // Auxiliary stream info
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2DemodL1PostPlp -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Struct for storing Layer 1 Post per PLP
//
struct DtDvbT2DemodL1PostPlp
{
    int  m_Id;                      // PLP ID: 0..255
    int  m_Type;                    // PLP type, see DTAPI_DVBT2_PLP_TYPE_x
    int  m_PayloadType;             // PLP payload type: 0..3
    int  m_FfFlag;                  // FF flag
    int  m_FirstRfIdx;              // 0..NumRf-1
    int  m_FirstFrameIdx;           // First frame in which PLP appears
    int  m_GroupId;                 // Group ID: 0..255
    int  m_CodeRate;                // PLP code rate, see DTAPI_DVBT2_COD_x
    int  m_Modulation;              // PLP modulation, see DTAPI_DVBT2_BPSK/...
    int  m_Rotation;                // PLP rotation yes/no
    int  m_FecType;                 // PLP FEC type: 0=LDPC 16K, 1=LDPC 64K
    int  m_NumBlocks;               // PLP_NUM_BLOCKS_MAX: Maximum number of FEC blocks
                                    // contained in one interleaving frame
    int  m_FrameInterval;           // The PLP appears every m_FrameInterval frames
    int  m_TimeIlLength;            // Time interleaver length: 0..255
    int  m_TimeIlType;              // Time interleaver type: 0 or 1
    int  m_InBandAFlag;             // IN_BAND_A_FLAG is used yes/no
    // V1.2.1 spec revision
    int  m_InBandBFlag;             // IN_BAND_B_FLAG is used yes/no
    int  m_Reserved1;               // Reserved field, may be used for bias balancing
    int  m_PlpMode;                 // PLP mode: 0..3
    int  m_Static;                  // Static flag: 0 or 1=Configuration changes only at
                                    // super frame boundaries
    int  m_StaticPadding;           // Static padding flag: 0 or 1=BBFRAME padding is not
                                    // used
    DtDvbT2DemodL1PostPlp();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2DemodRfPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Struct for storing the TFS RF channels information from Layer 1 Post 
//
struct DtDvbT2DemodRfPars
{
    int  m_RfIdx;                   // Index of the RF-frequency
    int  m_Frequency;               // Centre frequency in Hz
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2DemodL1Data -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Struct for storing the DVB-T2 layer 1 data
//
struct DtDvbT2DemodL1Data
{
    // P1 Info
    struct DtDvbT2DemodL1P1
    {
        bool  m_Valid;              // True if P1 was found
        int  m_FftMode;             // FFT mode, see DVBT2_FFT_x
        int  m_Miso;                // MISO used
        int  m_Fef;                 // FEF used
        int  m_T2Profile;           // DVB-T2 profile. See DVBT2_PROFILE_x
    } m_P1;

    // L1-pre info
    struct DtDvbT2DemodL1Pre
    {
        bool  m_Valid;              // True if L1 pre was correctly demodulated

        int  m_Type;                // Stream type within the current T2 super-frame
        int  m_BwtExt;              // Bandwidth extension
        int  m_S1;                  // S1 signalling. P1 S1. 
        int  m_S2;                  // S2 signalling. P1 S2.
        int  m_L1Repetition;        // L1 repetition
        int  m_GuardInterval;       // Guard interval, see DVBT2_GI_x
        int  m_Papr;                // PAPR. see DVBT2_PAPR_x
        int  m_L1Modulation;        // L1 modulation, see DVBT2_BPSK/...
        int  m_L1CodeRate;          // L1 coderate, see DTAPI_DVBT2_COD_x
        int  m_L1FecType;           // L1 FEC type: 0=LDPC 16K, 1=LDPC 64K
        int  m_L1PostSize;          // Size of the L1-post in OFDM cells
        int  m_l1PostInfoSize;      // L1-post info size = 
                                    //              L1-post configurable+dynamic+extension
        int  m_PilotPattern;        // Pilot pattern: 1..8
        int  m_TxIdAvailability;    // The Tx ID
        int  m_CellId;              // Cell ID: 0..0xFFFF
        int  m_NetworkId;           // Network ID: 0..0xFFFF
        int  m_T2SystemId;          // T2 System ID: 0..0xFFFF
        int  m_NumT2Frames;         // Number of T2-frames per superframe: 1..255
        int  m_NumDataSyms;         // Number of data symbols
        int  m_RegenFlag;           // Regeneration count indicator
        int  m_L1PostExt;           // L1-post extensions enabled
        int  m_NumRfChans;          // Number of RF channels: 1..7
        int  m_CurrentRfIdx;        // Current RF index: 0..m_NumRfChans-1
        int  m_T2Version;           // DVB-T2 spec version, see DVBT2_VERSION_x
        int  m_L1PostScrambling;    // L1 Post scrambling
        int  m_T2BaseLite;          // Indicates whether T2 lite is used in a base
                                    // profile stream
    } m_L1Pre;

    // L1-post info
    struct DtDvbT2DemodL1Post
    {
        bool  m_Valid;              // True if L1 post was correctly demodulated
        int  m_NumSubslices;        // Number of subslices per T2-frame (for type2 PLPs)
        int  m_NumPlps;             // Number of PLPs
        int  m_NumAux;              // Number of auxiliary streams

        // TFS RF-channels
        std::vector<DtDvbT2DemodRfPars>  m_RfChanFreqs;   

        // FEF info is meaningful if m_P1.m_Fef = true
        int  m_FefType;             // FEF type: 0..15
        int  m_FefLength;           // FEF length in number of samples
        int  m_FefInterval;         // FEF Interval

        // PLPs
        std::vector<DtDvbT2DemodL1PostPlp>  m_Plps;    

        // Auxiliary stream signalling information
        std::vector<DtDvbT2DemodAuxPars>  m_AuxPars;   

    } m_L1Post;

    DtDvbT2DemodL1Data();

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ ISDBT-TMM +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

#define DTAPI_ISDBT_NUM_TS_MAX  14     
//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbTmmPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// ISDB-Tmm parameters including per-layer parameters
//
struct DtIsdbTmmPars
{
    int  m_Bandwidth;
    int  m_SubChannel;
    int  m_NumTss;
    DtVirtualOutPars  m_VirtOutput;  // Virtual Output parameters(Optional) 
    DtIsdbtPars  m_Tss[DTAPI_ISDBT_NUM_TS_MAX];
    DtPlpInpPars  m_TsInputs[DTAPI_ISDBT_NUM_TS_MAX];

    // Member function
    DtIsdbTmmPars() { Init(); }
    DTAPI_RESULT  CheckValidity();
    int  NumSegm();
    void Init();
    DTAPI_RESULT SetSegmentFormat(int TsIdx, int SegmFormat);

    bool  operator == (DtIsdbTmmPars& Rhs);
    bool  operator != (DtIsdbTmmPars& Rhs);
};


//+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ Advanced Demodulator +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtStreamType -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Classifies the type of the stream
//
enum DtStreamType 
{
    STREAM_CONSTEL,                 // Constellation points
    STREAM_DAB,                     // DAB stream
    STREAM_DABETI,                  // DAB Ensemble Transport Interface (NI, G.703)
    STREAM_DABFIC,                  // DAB Fast Information Channel
    STREAM_DVBC2,                   // DVB-C2 stream (Transport Stream packets)
    STREAM_DVBC2_BBFRAME,           // DVB-C2 stream base-band frames
    STREAM_DVBT,                    // DVB-T stream
    STREAM_DVBT2,                   // DVB-T2 stream (Transport Stream packets)
    STREAM_DVBT2_BBFRAME,           // DVB-T2 stream base band frames
    STREAM_DVBT2_GSE,               // DVB-T2 stream GSE-packets
    STREAM_IMPRESP,                 // Impulse response
    STREAM_ISDBT,                   // ISDB-T stream
    STREAM_MER,                     // MER
    STREAM_SPECTRUM,                // Spectrum
    STREAM_T2MI,                    // DVB-T2 stream
    STREAM_TF_ABS,                  // Transfer function absolute
    STREAM_TF_PHASE,                // Transfer function phase
    STREAM_TF_GROUPDELAY            // Transfer function group delay
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtConstelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure specifies the parameters for a stream of constellation points
//
struct DtConstelPars
{
    int  m_Period;                  // Minimum period between callbacks in ms
    int  m_ConstellationType;       // 0: Constellation per PLP
                                    // 1: Constellation per carrier (after equalization)
    int  m_Index;                   // Index of the PLP or carrier
    int  m_MaxNumPoints;            // Maximum number of constellation points
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbTStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure specifies the selection parameters for a DVB-T transport stream
//
struct DtDvbTStreamSelPars
{
    // No selection parameters yet
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbTTpsInfo -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// DVB-T Transmission Parameter Signalling (TPS) information structure
//
struct DtDvbTTpsInfo
{
    int  m_LengthIndicator;         // TPS length indicator  
    int  m_Constellation;           // Constellation (DTAPI_MOD_DVBT_xxx)
    int  m_HpCodeRate;              // High Priority code rate (DTAPI_MOD_xxx)
    int  m_LpCodeRate;              // Low Priority code rate(DTAPI_MOD_xxx)
    int  m_Guard;                   // Guard interval (DTAPI_MOD_DVBT_G_xx)
    int  m_Interleaving;            // Interleaving (DTAPI_MOD_DVBT_xxx)
    int  m_Mode;                    // Transmission mode (DTAPI_MOD_DVBT_xK)
    int  m_Hierarchy;               // Hierarchy   (DTAPI_MOD_DVBT_HARCHY_xxx)
    int  m_CellId;                  // CellId or -1 when not present
    int  m_HpS48S49;                // S48S49 of the odd frames  (DTAPI_MOD_DVBT_Sxx)
    int  m_LpS48S49;                // S48S49 of the even frames (DTAPI_MOD_DVBT_Sxx)
    int  m_OddS50_S53;              // S50..S53 of the odd frames	
    int  m_EvenS50_S53;             // S50..S53 of the even frames	
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtDvbT2StreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure specifies the selection parameters for a DVB-T2 DVB-T2 GSE and 
// DVB-T2 BBFRAME stream
//
struct DtDvbT2StreamSelPars
{
    int  m_PlpId;                   // ID of the data PLP or DTAPI_DVBT2_PLP_ID_xxx
    int  m_CommonPlpId;             // ID of the common PLP or DTAPI_DVBT2_PLP_ID_xxx

    // Serialisation
    DTAPI_RESULT  FromXml(const std::wstring& XmlString);
    DTAPI_RESULT  ToXml(std::wstring& XmlString);
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtImpRespPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure specifies the parameters for an impulse-response stream
//
struct DtImpRespPars
{
    int  m_Period;                  // Minimum period bewteen callbacks in ms
    int  m_Channel;                 // Channel used for MISO
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtIsdbtStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure specifies the selection parameters for an ISDB-T stream
//
struct DtIsdbtStreamSelPars
{
    // No additional selection parameters
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtMeasurement -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Structure describing a set of measurement values. It used to pass measurements from 
// the advanced demodulator to the user application through user-supplied 
// DtWriteMeasFunc callback.
//
struct DtMeasurement
{
    DtStreamType  m_MeasurementType;// Type of measurement values
    __int64  m_TimeStamp;           // Timestamp in number of samples
    int  m_NumValues;               // Number of measurement values
    DtComplexFloat*  m_pMeasurement;// Measurement values
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtMerPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// This structure specifies the parameters for a MER stream.
//
struct DtMerPars
{
    int  m_Period;                  // Minimum period between callbacks in ms
};

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtSpectrumPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure specifies the parameters for a spectrum stream.
//
struct DtSpectrumPars
{
    int  m_Period;                  // Minimum time between callbacks in ms 
    int  m_FftLength;               // FFT length, must be a power of two
    int  m_AverageLength;           // Number of FFT blocks on wich the average is done
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtT2MiStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure specifies the selection parameters for a T2-MI transport stream
// containing a com-plete DVB-T2 stream.
//
struct DtT2MiStreamSelPars
{
    int  m_T2MiOutPid;              // T2-MI output PID
    int  m_T2MiTsRate;              // Rate in bps for the T2MI output.
                                    // If -1, output a variable bitrate T2MI stream 
                                    // (set_output_rate is not called in this case).
                                    // Otherwise, use m_T2MiTsRate to set the rate.
                                    // Padding null packets are used to reach the target
                                    // bitrate. The maximum T2-MI bitrate is 72Mbps.
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtTransFuncPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// This structure specifies the parameters for a transfer-function stream.
//
struct DtTransFuncPars
{
    int  m_Period;                  // Minimum time between callbacks in ms
    int  m_Channel;                 // Channel used for MISO
};

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtStreamSelPars -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
// Structure for streaming data selection
//
struct DtStreamSelPars
{
    intptr_t  m_Id;                 // Unique stream identifier
    DtStreamType  m_StreamType;     // Stream selection type

    union {
        // Selection parameters for demodulated streams
        DtDvbC2StreamSelPars  m_DvbC2;
        DtDvbTStreamSelPars  m_DvbT;
        DtDvbT2StreamSelPars  m_DvbT2;
        DtIsdbtStreamSelPars  m_Isdbt;
        DtT2MiStreamSelPars  m_T2Mi;
        DtDabStreamSelPars  m_Dab;
        DtDabEtiStreamSelPars  m_DabEti;
        DtDabFicStreamSelPars  m_DabFic;

        // Parameters for streams of measurement values
        DtConstelPars  m_Constel;
        DtImpRespPars  m_ImpResp;
        DtMerPars  m_Mer;
        DtSpectrumPars  m_Spectrum;
        DtTransFuncPars  m_TransFunc;
    } u;

    bool  operator == (DtStreamSelPars& Rhs);
    bool  operator != (DtStreamSelPars& Rhs);
};

// Software demodulator callback functions
typedef void  DtOutputRateChangedFunc(void *pOpaque, DtStreamSelPars&, int Bitrate);
typedef void  DtReadIqFunc(void* pOpaque, 
                                     unsigned char* pIqBuf, int IqBufSize, int& IqLength);
typedef void  DtWriteMeasFunc(void *pOpaque, DtStreamSelPars&, DtMeasurement*);
typedef void  DtWriteStreamFunc(void* pOpaque, DtStreamSelPars& StreamSel, 
                                                  const unsigned char* pData, int Length);

//-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtAdvDemod -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-
//
// Class representing an advanced demodulator. 
// DtAdvDemod can be considered a specialized in-put channel.
//
class DtAdvDemod
{
public:
    DtAdvDemod();
    virtual ~DtAdvDemod();
private:
    // No implementation is provided for the copy constructor
    DtAdvDemod(const DtAdvDemod&);

public:
    DtHwFuncDesc  m_HwFuncDesc;     // Hardware function descriptor

    // Convenience functions
public:
    int  Category(void)         { return m_HwFuncDesc.m_DvcDesc.m_Category; }
    int  FirmwareVersion(void)  { return m_HwFuncDesc.m_DvcDesc.m_FirmwareVersion; }
    bool  IsAttached(void)      { return m_pAdvDemod != NULL; }
    int  TypeNumber(void)       { return m_HwFuncDesc.m_DvcDesc.m_TypeNumber; }

public:
    DTAPI_RESULT  AttachToPort(DtDevice* pDtDvc, int Port,
                                               bool Exclusive=true, bool ProbeOnly=false);
    DTAPI_RESULT  AttachVirtual(DtDevice* pDtDvc,
                                                DtReadIqFunc* pReadIqFunc, void* pOpaque);
    DTAPI_RESULT  ClearFlags(int Latched);
    DTAPI_RESULT  CloseStream(intptr_t Id);
    DTAPI_RESULT  Detach(int DetachMode);
    DTAPI_RESULT  GetDemodControl(DtDemodPars* pDemodPars);
    DTAPI_RESULT  GetDescriptor(DtHwFuncDesc& HwFunDesc);
    DTAPI_RESULT  GetFlags(int& Flags, int& Latched);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue, __int64& ParXtra0);
    DTAPI_RESULT  GetIoConfig(int Group, int& Value, int& SubValue,
                                                    __int64& ParXtra0, __int64& ParXtra1);
    DTAPI_RESULT  GetPars(int Count, DtPar* pPars);    
    DTAPI_RESULT  GetRxControl(int& RxControl);
    DTAPI_RESULT  GetStatistics(int Count, DtStatistic* pStatistics);
    DTAPI_RESULT  GetStatistic(int Type, int& Statistic);
    DTAPI_RESULT  GetStatistic(int Type, double& Statistic);
    DTAPI_RESULT  GetStatistic(int Type, bool& Statistic);
    DTAPI_RESULT  GetStreamSelection(std::vector<DtStreamSelPars>& StreamSelList);
    DTAPI_RESULT  GetSupportedPars(int& NumPars, DtPar* pPars);
    DTAPI_RESULT  GetSupportedStatistics(int& Count, DtStatistic* pStatistics);
    DTAPI_RESULT  GetTsRateBps(intptr_t Id, int& TsRate);
    DTAPI_RESULT  GetTunerFrequency(__int64& FreqHz);
    DTAPI_RESULT  LedControl(int LedControl);
    DTAPI_RESULT  OpenStream(DtStreamSelPars StreamSel);
    DTAPI_RESULT  RegisterCallback(DtOutputRateChangedFunc* pCallback, void* pOpaque);
    DTAPI_RESULT  RegisterCallback(DtWriteStreamFunc* pCallback, void* pOpaque);
    DTAPI_RESULT  RegisterCallback(DtWriteMeasFunc* pCallback, void* pOpaque);
    DTAPI_RESULT  Reset(int ResetMode);
    DTAPI_RESULT  SetAntPower(int AntPower);
    DTAPI_RESULT  SetDemodControl(DtDemodPars *pDemodPars);
    DTAPI_RESULT  SetIoConfig(int Group, int Value, int SubValue,
                                            __int64 ParXtra0 = -1, __int64 ParXtra1 = -1);
    DTAPI_RESULT  SetPars(int Count, DtPar* pPars); 
    DTAPI_RESULT  SetRxControl(int RxControl);
    DTAPI_RESULT  SetTunerFrequency(__int64 FreqHz);
    DTAPI_RESULT  Tune(__int64 FreqHz, int ModType,
                                                int ParXtra0, int ParXtra1, int ParXtra2);
    DTAPI_RESULT  Tune(__int64 FreqHz, DtDemodPars *pDemodPars);

protected:                          
    AdvDemod*  m_pAdvDemod;         // Advanced demodulation channel implementation
    bool  m_IsAttachedToVirtual;    // Attached to virtual input port?

    // Encapsulated data
private:
    IXpMutex*  m_pMTLock;           // Multi-threading lock for get/read functions
    void*  m_pDetachLockCount;
    int  m_Port;
    bool  m_WantToDetach;
    
// Private helper functions
private:
    DTAPI_RESULT  DetachLock(void);
    DTAPI_RESULT  DetachUnlock(void);
    DTAPI_RESULT  ReadAccessLock(void);
    DTAPI_RESULT  ReadAccessUnlock(void);
};

// Forward declaration for use in DtRs422Channel
class Rs422Channel;

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.- DtRs422Channel -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
//
class DtRs422Channel
{
public:
    DtRs422Channel();
    virtual ~DtRs422Channel();
private:
    // No implementation is provided for the copy constructor
    DtRs422Channel(const DtRs422Channel&);

public:
    DTAPI_RESULT  AttachToPort(DtDevice* pDtDvc, int Port,
                                               bool Exclusive=true, bool ProbeOnly=false);
    DTAPI_RESULT  Detach();
    DTAPI_RESULT  Flush();
    DTAPI_RESULT  Read(char* pBuffer, int NumBytesToRead, int Timeout, int& NumBytesRead);
    DTAPI_RESULT  Write(char* pBuffer, int NumBytesToWrite, bool Blocking=true);

protected:
    Rs422Channel*  m_pRs422Channel;
    void*  m_pDetachLockCount;
    bool  m_WantToDetach;

// Private helper functions
private:
    DTAPI_RESULT  DetachLock(void);
    DTAPI_RESULT  DetachUnlock(void);
};

} // namespace Dtapi

#ifndef _NO_USING_NAMESPACE_DTAPI
using namespace Dtapi;
#endif

#endif //#ifndef __DTAPI_H

