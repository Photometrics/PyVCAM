###############################################################################
# File: constants.py
# Author: Cameron Smith
# Date of Last Edit: 2025-06-05
#
# Purpose: To maintain the naming conventions used with pvcam.h for Python
#          scripts.
#
# Notes: This file is generated by constants_generator.py. See that script for
#        details about implementation. Please do not alter this file. Instead,
#        make any additional changes to the constants_generator.py if
#        additional data is needed.
#
# Bugs: [See constants_generator.py]
###############################################################################
import ctypes

### DEFINES ###

MAX_CAM = 16
CAM_NAME_LEN = 32
PARAM_NAME_LEN = 32
ERROR_MSG_LEN = 255
CCD_NAME_LEN = 17
MAX_ALPHA_SER_NUM_LEN = 32
MAX_PP_NAME_LEN = 32
MAX_SYSTEM_NAME_LEN = 32
MAX_VENDOR_NAME_LEN = 32
MAX_PRODUCT_NAME_LEN = 32
MAX_CAM_PART_NUM_LEN = 32
MAX_GAIN_NAME_LEN = 32
MAX_SPDTAB_NAME_LEN = 32
MAX_CAM_SYSTEMS_INFO_LEN = 1024
PP_MAX_PARAMETERS_PER_FEATURE = 10
PL_MD_FRAME_SIGNATURE = 5328208
PL_MD_EXT_TAGS_MAX_SUPPORTED = 255
TYPE_INT16 = 1
TYPE_INT32 = 2
TYPE_FLT64 = 4
TYPE_UNS8 = 5
TYPE_UNS16 = 6
TYPE_UNS32 = 7
TYPE_UNS64 = 8
TYPE_ENUM = 9
TYPE_BOOLEAN = 11
TYPE_INT8 = 12
TYPE_CHAR_PTR = 13
TYPE_VOID_PTR = 14
TYPE_VOID_PTR_PTR = 15
TYPE_INT64 = 16
TYPE_SMART_STREAM_TYPE = 17
TYPE_SMART_STREAM_TYPE_PTR = 18
TYPE_FLT32 = 19
TYPE_RGN_TYPE = 20
TYPE_RGN_TYPE_PTR = 21
TYPE_RGN_LIST_TYPE = 22
TYPE_RGN_LIST_TYPE_PTR = 23
CLASS0 = 0
CLASS2 = 2
CLASS3 = 3
PARAM_DD_INFO_LENGTH = ((CLASS0<<16) + (TYPE_INT16<<24) + 1)
PARAM_DD_VERSION = ((CLASS0<<16) + (TYPE_UNS16<<24) + 2)
PARAM_DD_RETRIES = ((CLASS0<<16) + (TYPE_UNS16<<24) + 3)
PARAM_DD_TIMEOUT = ((CLASS0<<16) + (TYPE_UNS16<<24) + 4)
PARAM_DD_INFO = ((CLASS0<<16) + (TYPE_CHAR_PTR<<24) + 5)
PARAM_CAM_INTERFACE_TYPE = ((CLASS0<<16) + (TYPE_ENUM<<24) + 10)
PARAM_CAM_INTERFACE_MODE = ((CLASS0<<16) + (TYPE_ENUM<<24) + 11)
PARAM_ADC_OFFSET = ((CLASS2<<16) + (TYPE_INT16<<24)     + 195)
PARAM_CHIP_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 129)
PARAM_SYSTEM_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 130)
PARAM_VENDOR_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 131)
PARAM_PRODUCT_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 132)
PARAM_CAMERA_PART_NUMBER = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 133)
PARAM_COOLING_MODE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 214)
PARAM_PREAMP_DELAY = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 502)
PARAM_COLOR_MODE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 504)
PARAM_MPP_CAPABLE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 224)
PARAM_PREAMP_OFF_CONTROL = ((CLASS2<<16) + (TYPE_UNS32<<24)     + 507)
PARAM_PREMASK = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  53)
PARAM_PRESCAN = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  55)
PARAM_POSTMASK = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  54)
PARAM_POSTSCAN = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  56)
PARAM_PIX_PAR_DIST = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 500)
PARAM_PIX_PAR_SIZE = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  63)
PARAM_PIX_SER_DIST = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 501)
PARAM_PIX_SER_SIZE = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  62)
PARAM_SUMMING_WELL = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 505)
PARAM_FWELL_CAPACITY = ((CLASS2<<16) + (TYPE_UNS32<<24)     + 506)
PARAM_PAR_SIZE = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  57)
PARAM_SER_SIZE = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  58)
PARAM_ACCUM_CAPABLE = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 538)
PARAM_FLASH_DWNLD_CAPABLE = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 539)
PARAM_READOUT_TIME = ((CLASS2<<16) + (TYPE_FLT64<<24)     + 179)
PARAM_CLEARING_TIME = ((CLASS2<<16) + (TYPE_INT64<<24)     + 180)
PARAM_POST_TRIGGER_DELAY = ((CLASS2<<16) + (TYPE_INT64<<24)     + 181)
PARAM_PRE_TRIGGER_DELAY = ((CLASS2<<16) + (TYPE_INT64<<24)     + 182)
PARAM_CLEAR_CYCLES = ((CLASS2<<16) + (TYPE_UNS16<<24)     +  97)
PARAM_CLEAR_MODE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 523)
PARAM_FRAME_CAPABLE = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 509)
PARAM_PMODE = ((CLASS2<<16) + (TYPE_ENUM <<24)     + 524)
PARAM_TEMP = ((CLASS2<<16) + (TYPE_INT16<<24)     + 525)
PARAM_TEMP_SETPOINT = ((CLASS2<<16) + (TYPE_INT16<<24)     + 526)
PARAM_CAM_FW_VERSION = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 532)
PARAM_HEAD_SER_NUM_ALPHA = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 533)
PARAM_PCI_FW_VERSION = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 534)
PARAM_FAN_SPEED_SETPOINT = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 710)
PARAM_CAM_SYSTEMS_INFO = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 536)
PARAM_EXPOSURE_MODE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 535)
PARAM_EXPOSE_OUT_MODE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 560)
PARAM_BIT_DEPTH = ((CLASS2<<16) + (TYPE_INT16<<24)     + 511)
PARAM_BIT_DEPTH_HOST = ((CLASS2<<16) + (TYPE_INT16<<24)     + 551)
PARAM_IMAGE_FORMAT = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 248)
PARAM_IMAGE_FORMAT_HOST = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 552)
PARAM_IMAGE_COMPRESSION = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 249)
PARAM_IMAGE_COMPRESSION_HOST = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 253)
PARAM_SCAN_MODE = ((CLASS3<<16) + (TYPE_ENUM<<24)      + 250)
PARAM_SCAN_DIRECTION = ((CLASS3<<16) + (TYPE_ENUM<<24)      + 251)
PARAM_SCAN_DIRECTION_RESET = ((CLASS3<<16) + (TYPE_BOOLEAN<<24)      + 252)
PARAM_SCAN_LINE_DELAY = ((CLASS3<<16) + (TYPE_UNS16<<24)      + 253)
PARAM_SCAN_LINE_TIME = ((CLASS3<<16) + (TYPE_INT64<<24)      + 254)
PARAM_SCAN_WIDTH = ((CLASS3<<16) + (TYPE_UNS16<<24)      + 255)
PARAM_HOST_FRAME_ROTATE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 256)
PARAM_FRAME_ROTATE = (PARAM_HOST_FRAME_ROTATE)
PARAM_HOST_FRAME_FLIP = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 257)
PARAM_FRAME_FLIP = (PARAM_HOST_FRAME_FLIP)
PARAM_HOST_FRAME_SUMMING_ENABLED = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 258)
PARAM_HOST_FRAME_SUMMING_COUNT = ((CLASS2<<16) + (TYPE_UNS32<<24)     + 259)
PARAM_HOST_FRAME_SUMMING_FORMAT = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 260)
PARAM_HOST_FRAME_DECOMPRESSION_ENABLED = ((CLASS2<<16) + (TYPE_BOOLEAN<<24) + 261)
PARAM_GAIN_INDEX = ((CLASS2<<16) + (TYPE_INT16<<24)     + 512)
PARAM_SPDTAB_INDEX = ((CLASS2<<16) + (TYPE_INT16<<24)     + 513)
PARAM_GAIN_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)  + 514)
PARAM_SPDTAB_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24)   + 515)
PARAM_READOUT_PORT = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 247)
PARAM_PIX_TIME = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 516)
PARAM_SHTR_CLOSE_DELAY = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 519)
PARAM_SHTR_OPEN_DELAY = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 520)
PARAM_SHTR_OPEN_MODE = ((CLASS2<<16) + (TYPE_ENUM <<24)     + 521)
PARAM_SHTR_STATUS = ((CLASS2<<16) + (TYPE_ENUM <<24)     + 522)
PARAM_IO_ADDR = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 527)
PARAM_IO_TYPE = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 528)
PARAM_IO_DIRECTION = ((CLASS2<<16) + (TYPE_ENUM<<24)      + 529)
PARAM_IO_STATE = ((CLASS2<<16) + (TYPE_FLT64<<24)     + 530)
PARAM_IO_BITDEPTH = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 531)
PARAM_GAIN_MULT_FACTOR = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 537)
PARAM_GAIN_MULT_ENABLE = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)   + 541)
PARAM_PP_FEAT_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24) +  542)
PARAM_PP_INDEX = ((CLASS2<<16) + (TYPE_INT16<<24)    +  543)
PARAM_ACTUAL_GAIN = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 544)
PARAM_PP_PARAM_INDEX = ((CLASS2<<16) + (TYPE_INT16<<24)    +  545)
PARAM_PP_PARAM_NAME = ((CLASS2<<16) + (TYPE_CHAR_PTR<<24) +  546)
PARAM_PP_PARAM = ((CLASS2<<16) + (TYPE_UNS32<<24)    +  547)
PARAM_READ_NOISE = ((CLASS2<<16) + (TYPE_UNS16<<24)     + 548)
PARAM_PP_FEAT_ID = ((CLASS2<<16) + (TYPE_UNS16<<24)    +  549)
PARAM_PP_PARAM_ID = ((CLASS2<<16) + (TYPE_UNS16<<24)    +  550)
PARAM_SMART_STREAM_MODE_ENABLED = ((CLASS2<<16) + (TYPE_BOOLEAN<<24)  +  700)
PARAM_SMART_STREAM_MODE = ((CLASS2<<16) + (TYPE_UNS16<<24)    +  701)
PARAM_SMART_STREAM_EXP_PARAMS = ((CLASS2<<16) + (TYPE_VOID_PTR<<24) +  702)
PARAM_SMART_STREAM_DLY_PARAMS = ((CLASS2<<16) + (TYPE_VOID_PTR<<24) +  703)
PARAM_EXP_TIME = ((CLASS3<<16) + (TYPE_UNS16<<24)     +   1)
PARAM_EXP_RES = ((CLASS3<<16) + (TYPE_ENUM<<24)      +   2)
PARAM_EXP_RES_INDEX = ((CLASS3<<16) + (TYPE_UNS16<<24)     +   4)
PARAM_EXPOSURE_TIME = ((CLASS3<<16) + (TYPE_UNS64<<24)     +   8)
PARAM_BOF_EOF_ENABLE = ((CLASS3<<16) + (TYPE_ENUM<<24)      +   5)
PARAM_BOF_EOF_COUNT = ((CLASS3<<16) + (TYPE_UNS32<<24)     +   6)
PARAM_BOF_EOF_CLR = ((CLASS3<<16) + (TYPE_BOOLEAN<<24)   +   7)
PARAM_CIRC_BUFFER = ((CLASS3<<16) + (TYPE_BOOLEAN<<24)   + 299)
PARAM_FRAME_BUFFER_SIZE = ((CLASS3<<16) + (TYPE_UNS64<<24)     + 300)
PARAM_BINNING_SER = ((CLASS3<<16) + (TYPE_ENUM<<24)      + 165)
PARAM_BINNING_PAR = ((CLASS3<<16) + (TYPE_ENUM<<24)      + 166)
PARAM_METADATA_ENABLED = ((CLASS3<<16) + (TYPE_BOOLEAN<<24)   + 168)
PARAM_METADATA_RESET_TIMESTAMP = ((CLASS3<<16) + (TYPE_BOOLEAN<<24) + 176)
PARAM_ROI_COUNT = ((CLASS3<<16) + (TYPE_UNS16  <<24)   + 169)
PARAM_ROI = ((CLASS3<<16) + (TYPE_RGN_TYPE<<24)      + 1)
PARAM_CENTROIDS_ENABLED = ((CLASS3<<16) + (TYPE_BOOLEAN<<24)   + 170)
PARAM_CENTROIDS_RADIUS = ((CLASS3<<16) + (TYPE_UNS16  <<24)   + 171)
PARAM_CENTROIDS_COUNT = ((CLASS3<<16) + (TYPE_UNS16  <<24)   + 172)
PARAM_CENTROIDS_MODE = ((CLASS3<<16) + (TYPE_ENUM   <<24)   + 173)
PARAM_CENTROIDS_BG_COUNT = ((CLASS3<<16) + (TYPE_ENUM   <<24)   + 174)
PARAM_CENTROIDS_THRESHOLD = ((CLASS3<<16) + (TYPE_UNS32  <<24)   + 175)
PARAM_TRIGTAB_SIGNAL = ((CLASS3<<16) + (TYPE_ENUM<<24)      + 180)
PARAM_LAST_MUXED_SIGNAL = ((CLASS3<<16) + (TYPE_UNS8<<24)      + 181)
PARAM_FRAME_DELIVERY_MODE = ((CLASS3<<16) + (TYPE_ENUM <<24)     + 400)

### ENUMS ###

# PL_OPEN_MODES
OPEN_EXCLUSIVE = 0

# PL_COOL_MODES
NORMAL_COOL = 0
CRYO_COOL = NORMAL_COOL + 1
NO_COOL = CRYO_COOL + 1

# PL_MPP_MODES
MPP_UNKNOWN = 0
MPP_ALWAYS_OFF = MPP_UNKNOWN + 1
MPP_ALWAYS_ON = MPP_ALWAYS_OFF + 1
MPP_SELECTABLE = MPP_ALWAYS_ON + 1

# PL_SHTR_MODES
SHTR_FAULT = 0
SHTR_OPENING = SHTR_FAULT + 1
SHTR_OPEN = SHTR_OPENING + 1
SHTR_CLOSING = SHTR_OPEN + 1
SHTR_CLOSED = SHTR_CLOSING + 1
SHTR_UNKNOWN = SHTR_CLOSED + 1

# PL_PMODES
PMODE_NORMAL = 0
PMODE_FT = PMODE_NORMAL + 1
PMODE_MPP = PMODE_FT + 1
PMODE_FT_MPP = PMODE_MPP + 1
PMODE_ALT_NORMAL = PMODE_FT_MPP + 1
PMODE_ALT_FT = PMODE_ALT_NORMAL + 1
PMODE_ALT_MPP = PMODE_ALT_FT + 1
PMODE_ALT_FT_MPP = PMODE_ALT_MPP + 1

# PL_COLOR_MODES
COLOR_NONE =  0
COLOR_RESERVED =  1
COLOR_RGGB =  2
COLOR_GRBG = COLOR_RGGB + 1
COLOR_GBRG = COLOR_GRBG + 1
COLOR_BGGR = COLOR_GBRG + 1

# PL_IMAGE_FORMATS
PL_IMAGE_FORMAT_MONO16 =  0
PL_IMAGE_FORMAT_BAYER16 = PL_IMAGE_FORMAT_MONO16 + 1
PL_IMAGE_FORMAT_MONO8 = PL_IMAGE_FORMAT_BAYER16 + 1
PL_IMAGE_FORMAT_BAYER8 = PL_IMAGE_FORMAT_MONO8 + 1
PL_IMAGE_FORMAT_MONO24 = PL_IMAGE_FORMAT_BAYER8 + 1
PL_IMAGE_FORMAT_BAYER24 = PL_IMAGE_FORMAT_MONO24 + 1
PL_IMAGE_FORMAT_RGB24 = PL_IMAGE_FORMAT_BAYER24 + 1
PL_IMAGE_FORMAT_RGB48 = PL_IMAGE_FORMAT_RGB24 + 1
PL_IMAGE_FORMAT_RGB72 = PL_IMAGE_FORMAT_RGB48 + 1
PL_IMAGE_FORMAT_MONO32 = PL_IMAGE_FORMAT_RGB72 + 1
PL_IMAGE_FORMAT_BAYER32 = PL_IMAGE_FORMAT_MONO32 + 1
PL_IMAGE_FORMAT_RGB96 = PL_IMAGE_FORMAT_BAYER32 + 1

# PL_IMAGE_COMPRESSIONS
PL_IMAGE_COMPRESSION_NONE =  0
PL_IMAGE_COMPRESSION_RESERVED8 =  8
PL_IMAGE_COMPRESSION_BITPACK9 = PL_IMAGE_COMPRESSION_RESERVED8 + 1
PL_IMAGE_COMPRESSION_BITPACK10 = PL_IMAGE_COMPRESSION_BITPACK9 + 1
PL_IMAGE_COMPRESSION_BITPACK11 = PL_IMAGE_COMPRESSION_BITPACK10 + 1
PL_IMAGE_COMPRESSION_BITPACK12 = PL_IMAGE_COMPRESSION_BITPACK11 + 1
PL_IMAGE_COMPRESSION_BITPACK13 = PL_IMAGE_COMPRESSION_BITPACK12 + 1
PL_IMAGE_COMPRESSION_BITPACK14 = PL_IMAGE_COMPRESSION_BITPACK13 + 1
PL_IMAGE_COMPRESSION_BITPACK15 = PL_IMAGE_COMPRESSION_BITPACK14 + 1
PL_IMAGE_COMPRESSION_RESERVED16 =  16
PL_IMAGE_COMPRESSION_BITPACK17 = PL_IMAGE_COMPRESSION_RESERVED16 + 1
PL_IMAGE_COMPRESSION_BITPACK18 = PL_IMAGE_COMPRESSION_BITPACK17 + 1
PL_IMAGE_COMPRESSION_RESERVED24 =  24
PL_IMAGE_COMPRESSION_RESERVED32 =  32

# PL_FRAME_ROTATE_MODES
PL_FRAME_ROTATE_MODE_NONE =  0
PL_FRAME_ROTATE_MODE_90CW = PL_FRAME_ROTATE_MODE_NONE + 1
PL_FRAME_ROTATE_MODE_180CW = PL_FRAME_ROTATE_MODE_90CW + 1
PL_FRAME_ROTATE_MODE_270CW = PL_FRAME_ROTATE_MODE_180CW + 1

# PL_FRAME_FLIP_MODES
PL_FRAME_FLIP_MODE_NONE =  0
PL_FRAME_FLIP_MODE_X = PL_FRAME_FLIP_MODE_NONE + 1
PL_FRAME_FLIP_MODE_Y = PL_FRAME_FLIP_MODE_X + 1
PL_FRAME_FLIP_MODE_XY = PL_FRAME_FLIP_MODE_Y + 1

# PL_FRAME_SUMMING_FORMATS
PL_FRAME_SUMMING_FORMAT_16_BIT =  0
PL_FRAME_SUMMING_FORMAT_24_BIT = PL_FRAME_SUMMING_FORMAT_16_BIT + 1
PL_FRAME_SUMMING_FORMAT_32_BIT = PL_FRAME_SUMMING_FORMAT_24_BIT + 1

# PL_PARAM_ATTRIBUTES
ATTR_CURRENT = 0
ATTR_COUNT = ATTR_CURRENT + 1
ATTR_TYPE = ATTR_COUNT + 1
ATTR_MIN = ATTR_TYPE + 1
ATTR_MAX = ATTR_MIN + 1
ATTR_DEFAULT = ATTR_MAX + 1
ATTR_INCREMENT = ATTR_DEFAULT + 1
ATTR_ACCESS = ATTR_INCREMENT + 1
ATTR_AVAIL = ATTR_ACCESS + 1
ATTR_LIVE = ATTR_AVAIL + 1

# PL_PARAM_ACCESS
ACC_READ_ONLY =  1
ACC_READ_WRITE = ACC_READ_ONLY + 1
ACC_EXIST_CHECK_ONLY = ACC_READ_WRITE + 1
ACC_WRITE_ONLY = ACC_EXIST_CHECK_ONLY + 1

# PL_IO_TYPE
IO_TYPE_TTL = 0
IO_TYPE_DAC = IO_TYPE_TTL + 1

# PL_IO_DIRECTION
IO_DIR_INPUT = 0
IO_DIR_OUTPUT = IO_DIR_INPUT + 1
IO_DIR_INPUT_OUTPUT = IO_DIR_OUTPUT + 1

# PL_READOUT_PORTS
READOUT_PORT_0 =  0
READOUT_PORT_1 = READOUT_PORT_0 + 1
READOUT_PORT_2 = READOUT_PORT_1 + 1
READOUT_PORT_3 = READOUT_PORT_2 + 1

# PL_CLEAR_MODES
CLEAR_NEVER = 0
CLEAR_AUTO =  CLEAR_NEVER
CLEAR_PRE_EXPOSURE = CLEAR_AUTO + 1
CLEAR_PRE_SEQUENCE = CLEAR_PRE_EXPOSURE + 1
CLEAR_POST_SEQUENCE = CLEAR_PRE_SEQUENCE + 1
CLEAR_PRE_POST_SEQUENCE = CLEAR_POST_SEQUENCE + 1
CLEAR_PRE_EXPOSURE_POST_SEQ = CLEAR_PRE_POST_SEQUENCE + 1
MAX_CLEAR_MODE = CLEAR_PRE_EXPOSURE_POST_SEQ + 1

# PL_SHTR_OPEN_MODES
OPEN_NEVER = 0
OPEN_PRE_EXPOSURE = OPEN_NEVER + 1
OPEN_PRE_SEQUENCE = OPEN_PRE_EXPOSURE + 1
OPEN_PRE_TRIGGER = OPEN_PRE_SEQUENCE + 1
OPEN_NO_CHANGE = OPEN_PRE_TRIGGER + 1

# PL_EXPOSURE_MODES
TIMED_MODE = 0
STROBED_MODE = TIMED_MODE + 1
BULB_MODE = STROBED_MODE + 1
TRIGGER_FIRST_MODE = BULB_MODE + 1
FLASH_MODE = TRIGGER_FIRST_MODE + 1
VARIABLE_TIMED_MODE = FLASH_MODE + 1
INT_STROBE_MODE = VARIABLE_TIMED_MODE + 1
MAX_EXPOSE_MODE =  7
EXT_TRIG_INTERNAL =  (7 + 0) << 8
EXT_TRIG_TRIG_FIRST =  (7 + 1) << 8
EXT_TRIG_EDGE_RISING =  (7 + 2) << 8
EXT_TRIG_LEVEL =  (7 + 3) << 8
EXT_TRIG_SOFTWARE_FIRST =  (7 + 4) << 8
EXT_TRIG_SOFTWARE_EDGE =  (7 + 5) << 8
EXT_TRIG_LEVEL_OVERLAP =  (7 + 6) << 8
EXT_TRIG_LEVEL_PULSED =  (7 + 7) << 8


# PL_SW_TRIG_STATUSES
PL_SW_TRIG_STATUS_TRIGGERED =  0
PL_SW_TRIG_STATUS_IGNORED = PL_SW_TRIG_STATUS_TRIGGERED + 1

# PL_EXPOSE_OUT_MODES
EXPOSE_OUT_FIRST_ROW =  0
EXPOSE_OUT_ALL_ROWS = EXPOSE_OUT_FIRST_ROW + 1
EXPOSE_OUT_ANY_ROW = EXPOSE_OUT_ALL_ROWS + 1
EXPOSE_OUT_ROLLING_SHUTTER = EXPOSE_OUT_ANY_ROW + 1
EXPOSE_OUT_LINE_TRIGGER = EXPOSE_OUT_ROLLING_SHUTTER + 1
EXPOSE_OUT_GLOBAL_SHUTTER = EXPOSE_OUT_LINE_TRIGGER + 1
MAX_EXPOSE_OUT_MODE = EXPOSE_OUT_GLOBAL_SHUTTER + 1

# PL_FAN_SPEEDS
FAN_SPEED_HIGH = 0
FAN_SPEED_MEDIUM = FAN_SPEED_HIGH + 1
FAN_SPEED_LOW = FAN_SPEED_MEDIUM + 1
FAN_SPEED_OFF = FAN_SPEED_LOW + 1

# PL_TRIGTAB_SIGNALS
PL_TRIGTAB_SIGNAL_EXPOSE_OUT = 0

# PL_FRAME_DELIVERY_MODES
PL_FRAME_DELIVERY_MODE_MAX_FPS =  0
PL_FRAME_DELIVERY_MODE_CONSTANT_INTERVALS = PL_FRAME_DELIVERY_MODE_MAX_FPS + 1

# PL_CAM_INTERFACE_TYPES
PL_CAM_IFC_TYPE_UNKNOWN =  0
PL_CAM_IFC_TYPE_1394 =  0x100
PL_CAM_IFC_TYPE_1394_A = PL_CAM_IFC_TYPE_1394 + 1
PL_CAM_IFC_TYPE_1394_B = PL_CAM_IFC_TYPE_1394_A + 1
PL_CAM_IFC_TYPE_USB =  0x200
PL_CAM_IFC_TYPE_USB_1_1 = PL_CAM_IFC_TYPE_USB + 1
PL_CAM_IFC_TYPE_USB_2_0 = PL_CAM_IFC_TYPE_USB_1_1 + 1
PL_CAM_IFC_TYPE_USB_3_0 = PL_CAM_IFC_TYPE_USB_2_0 + 1
PL_CAM_IFC_TYPE_USB_3_1 = PL_CAM_IFC_TYPE_USB_3_0 + 1
PL_CAM_IFC_TYPE_PCI =  0x400
PL_CAM_IFC_TYPE_PCI_LVDS = PL_CAM_IFC_TYPE_PCI + 1
PL_CAM_IFC_TYPE_PCIE =  0x800
PL_CAM_IFC_TYPE_PCIE_LVDS = PL_CAM_IFC_TYPE_PCIE + 1
PL_CAM_IFC_TYPE_PCIE_X1 = PL_CAM_IFC_TYPE_PCIE_LVDS + 1
PL_CAM_IFC_TYPE_PCIE_X4 = PL_CAM_IFC_TYPE_PCIE_X1 + 1
PL_CAM_IFC_TYPE_PCIE_X8 = PL_CAM_IFC_TYPE_PCIE_X4 + 1
PL_CAM_IFC_TYPE_VIRTUAL =  0x1000
PL_CAM_IFC_TYPE_ETHERNET =  0x2000


# PL_CAM_INTERFACE_MODES
PL_CAM_IFC_MODE_UNSUPPORTED =  0
PL_CAM_IFC_MODE_CONTROL_ONLY = PL_CAM_IFC_MODE_UNSUPPORTED + 1
PL_CAM_IFC_MODE_IMAGING = PL_CAM_IFC_MODE_CONTROL_ONLY + 1

# PL_CENTROIDS_MODES
PL_CENTROIDS_MODE_LOCATE =  0
PL_CENTROIDS_MODE_TRACK = PL_CENTROIDS_MODE_LOCATE + 1
PL_CENTROIDS_MODE_BLOB = PL_CENTROIDS_MODE_TRACK + 1

# PL_SCAN_MODES
PL_SCAN_MODE_AUTO =  0
PL_SCAN_MODE_PROGRAMMABLE_LINE_DELAY = PL_SCAN_MODE_AUTO + 1
PL_SCAN_MODE_PROGRAMMABLE_SCAN_WIDTH = PL_SCAN_MODE_PROGRAMMABLE_LINE_DELAY + 1

# PL_SCAN_DIRECTIONS
PL_SCAN_DIRECTION_DOWN =  0
PL_SCAN_DIRECTION_UP = PL_SCAN_DIRECTION_DOWN + 1
PL_SCAN_DIRECTION_DOWN_UP = PL_SCAN_DIRECTION_UP + 1

# PP_FEATURE_IDS
PP_FEATURE_RING_FUNCTION = 0
PP_FEATURE_BIAS = PP_FEATURE_RING_FUNCTION + 1
PP_FEATURE_BERT = PP_FEATURE_BIAS + 1
PP_FEATURE_QUANT_VIEW = PP_FEATURE_BERT + 1
PP_FEATURE_BLACK_LOCK = PP_FEATURE_QUANT_VIEW + 1
PP_FEATURE_TOP_LOCK = PP_FEATURE_BLACK_LOCK + 1
PP_FEATURE_VARI_BIT = PP_FEATURE_TOP_LOCK + 1
PP_FEATURE_RESERVED = PP_FEATURE_VARI_BIT + 1
PP_FEATURE_DESPECKLE_BRIGHT_HIGH = PP_FEATURE_RESERVED + 1
PP_FEATURE_DESPECKLE_DARK_LOW = PP_FEATURE_DESPECKLE_BRIGHT_HIGH + 1
PP_FEATURE_DEFECTIVE_PIXEL_CORRECTION = PP_FEATURE_DESPECKLE_DARK_LOW + 1
PP_FEATURE_DYNAMIC_DARK_FRAME_CORRECTION = PP_FEATURE_DEFECTIVE_PIXEL_CORRECTION + 1
PP_FEATURE_HIGH_DYNAMIC_RANGE = PP_FEATURE_DYNAMIC_DARK_FRAME_CORRECTION + 1
PP_FEATURE_DESPECKLE_BRIGHT_LOW = PP_FEATURE_HIGH_DYNAMIC_RANGE + 1
PP_FEATURE_DENOISING = PP_FEATURE_DESPECKLE_BRIGHT_LOW + 1
PP_FEATURE_DESPECKLE_DARK_HIGH = PP_FEATURE_DENOISING + 1
PP_FEATURE_ENHANCED_DYNAMIC_RANGE = PP_FEATURE_DESPECKLE_DARK_HIGH + 1
PP_FEATURE_FRAME_SUMMING = PP_FEATURE_ENHANCED_DYNAMIC_RANGE + 1
PP_FEATURE_LARGE_CLUSTER_CORRECTION = PP_FEATURE_FRAME_SUMMING + 1
PP_FEATURE_FRAME_AVERAGING = PP_FEATURE_LARGE_CLUSTER_CORRECTION + 1
PP_FEATURE_MAX = PP_FEATURE_FRAME_AVERAGING + 1

# PP_PARAMETER_IDS
PP_PARAMETER_RF_FUNCTION =  (PP_FEATURE_RING_FUNCTION * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_BIAS_ENABLED =  (PP_FEATURE_BIAS * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_BIAS_LEVEL = PP_FEATURE_BIAS_ENABLED + 1
PP_FEATURE_BERT_ENABLED =  (PP_FEATURE_BERT * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_BERT_THRESHOLD = PP_FEATURE_BERT_ENABLED + 1
PP_FEATURE_QUANT_VIEW_ENABLED =  (PP_FEATURE_QUANT_VIEW * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_QUANT_VIEW_E = PP_FEATURE_QUANT_VIEW_ENABLED + 1
PP_FEATURE_BLACK_LOCK_ENABLED =  (PP_FEATURE_BLACK_LOCK * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_BLACK_LOCK_BLACK_CLIP = PP_FEATURE_BLACK_LOCK_ENABLED + 1
PP_FEATURE_TOP_LOCK_ENABLED =  (PP_FEATURE_TOP_LOCK * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_TOP_LOCK_WHITE_CLIP = PP_FEATURE_TOP_LOCK_ENABLED + 1
PP_FEATURE_VARI_BIT_ENABLED =  (PP_FEATURE_VARI_BIT * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_VARI_BIT_BIT_DEPTH = PP_FEATURE_VARI_BIT_ENABLED + 1
PP_FEATURE_DESPECKLE_BRIGHT_HIGH_ENABLED =  (PP_FEATURE_DESPECKLE_BRIGHT_HIGH * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DESPECKLE_BRIGHT_HIGH_THRESHOLD = PP_FEATURE_DESPECKLE_BRIGHT_HIGH_ENABLED + 1
PP_FEATURE_DESPECKLE_BRIGHT_HIGH_MIN_ADU_AFFECTED = PP_FEATURE_DESPECKLE_BRIGHT_HIGH_THRESHOLD + 1
PP_FEATURE_DESPECKLE_DARK_LOW_ENABLED =  (PP_FEATURE_DESPECKLE_DARK_LOW * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DESPECKLE_DARK_LOW_THRESHOLD = PP_FEATURE_DESPECKLE_DARK_LOW_ENABLED + 1
PP_FEATURE_DESPECKLE_DARK_LOW_MAX_ADU_AFFECTED = PP_FEATURE_DESPECKLE_DARK_LOW_THRESHOLD + 1
PP_FEATURE_DEFECTIVE_PIXEL_CORRECTION_ENABLED =  (PP_FEATURE_DEFECTIVE_PIXEL_CORRECTION * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DYNAMIC_DARK_FRAME_CORRECTION_ENABLED =  (PP_FEATURE_DYNAMIC_DARK_FRAME_CORRECTION * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_HIGH_DYNAMIC_RANGE_ENABLED =  (PP_FEATURE_HIGH_DYNAMIC_RANGE * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DESPECKLE_BRIGHT_LOW_ENABLED =  (PP_FEATURE_DESPECKLE_BRIGHT_LOW * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DESPECKLE_BRIGHT_LOW_THRESHOLD = PP_FEATURE_DESPECKLE_BRIGHT_LOW_ENABLED + 1
PP_FEATURE_DESPECKLE_BRIGHT_LOW_MAX_ADU_AFFECTED = PP_FEATURE_DESPECKLE_BRIGHT_LOW_THRESHOLD + 1
PP_FEATURE_DENOISING_ENABLED =  (PP_FEATURE_DENOISING * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DENOISING_NO_OF_ITERATIONS = PP_FEATURE_DENOISING_ENABLED + 1
PP_FEATURE_DENOISING_GAIN = PP_FEATURE_DENOISING_NO_OF_ITERATIONS + 1
PP_FEATURE_DENOISING_OFFSET = PP_FEATURE_DENOISING_GAIN + 1
PP_FEATURE_DENOISING_LAMBDA = PP_FEATURE_DENOISING_OFFSET + 1
PP_FEATURE_DESPECKLE_DARK_HIGH_ENABLED =  (PP_FEATURE_DESPECKLE_DARK_HIGH * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_DESPECKLE_DARK_HIGH_THRESHOLD = PP_FEATURE_DESPECKLE_DARK_HIGH_ENABLED + 1
PP_FEATURE_DESPECKLE_DARK_HIGH_MIN_ADU_AFFECTED = PP_FEATURE_DESPECKLE_DARK_HIGH_THRESHOLD + 1
PP_FEATURE_ENHANCED_DYNAMIC_RANGE_ENABLED =  (PP_FEATURE_ENHANCED_DYNAMIC_RANGE * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_FRAME_SUMMING_ENABLED =  (PP_FEATURE_FRAME_SUMMING * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_FRAME_SUMMING_COUNT = PP_FEATURE_FRAME_SUMMING_ENABLED + 1
PP_FEATURE_FRAME_SUMMING_32_BIT_MODE = PP_FEATURE_FRAME_SUMMING_COUNT + 1
PP_FEATURE_LARGE_CLUSTER_CORRECTION_ENABLED =  (PP_FEATURE_LARGE_CLUSTER_CORRECTION * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_FRAME_AVERAGING_ENABLED =  (PP_FEATURE_FRAME_AVERAGING * PP_MAX_PARAMETERS_PER_FEATURE)
PP_FEATURE_FRAME_AVERAGING_COUNT_FACTOR = PP_FEATURE_FRAME_AVERAGING_ENABLED + 1
PP_PARAMETER_ID_MAX = PP_FEATURE_FRAME_AVERAGING_COUNT_FACTOR + 1

# PL_SMT_MODES
SMTMODE_ARBITRARY_ALL =  0
SMTMODE_MAX = SMTMODE_ARBITRARY_ALL + 1

# PL_IMAGE_STATUSES
READOUT_NOT_ACTIVE = 0
EXPOSURE_IN_PROGRESS = READOUT_NOT_ACTIVE + 1
READOUT_IN_PROGRESS = EXPOSURE_IN_PROGRESS + 1
READOUT_COMPLETE = READOUT_IN_PROGRESS + 1
FRAME_AVAILABLE =  READOUT_COMPLETE
READOUT_FAILED = FRAME_AVAILABLE + 1
ACQUISITION_IN_PROGRESS = READOUT_FAILED + 1
MAX_CAMERA_STATUS = ACQUISITION_IN_PROGRESS + 1

# PL_CCS_ABORT_MODES
CCS_NO_CHANGE =  0
CCS_HALT = CCS_NO_CHANGE + 1
CCS_HALT_CLOSE_SHTR = CCS_HALT + 1
CCS_CLEAR = CCS_HALT_CLOSE_SHTR + 1
CCS_CLEAR_CLOSE_SHTR = CCS_CLEAR + 1
CCS_OPEN_SHTR = CCS_CLEAR_CLOSE_SHTR + 1
CCS_CLEAR_OPEN_SHTR = CCS_OPEN_SHTR + 1

# PL_IRQ_MODES
NO_FRAME_IRQS =  0
BEGIN_FRAME_IRQS = NO_FRAME_IRQS + 1
END_FRAME_IRQS = BEGIN_FRAME_IRQS + 1
BEGIN_END_FRAME_IRQS = END_FRAME_IRQS + 1

# PL_CIRC_MODES
CIRC_NONE =  0
CIRC_OVERWRITE = CIRC_NONE + 1
CIRC_NO_OVERWRITE = CIRC_OVERWRITE + 1

# PL_EXP_RES_MODES
EXP_RES_ONE_MILLISEC =  0
EXP_RES_ONE_MICROSEC = EXP_RES_ONE_MILLISEC + 1
EXP_RES_ONE_SEC = EXP_RES_ONE_MICROSEC + 1

# PL_SRC_MODES
SCR_PRE_OPEN_SHTR =  0
SCR_POST_OPEN_SHTR = SCR_PRE_OPEN_SHTR + 1
SCR_PRE_FLASH = SCR_POST_OPEN_SHTR + 1
SCR_POST_FLASH = SCR_PRE_FLASH + 1
SCR_PRE_INTEGRATE = SCR_POST_FLASH + 1
SCR_POST_INTEGRATE = SCR_PRE_INTEGRATE + 1
SCR_PRE_READOUT = SCR_POST_INTEGRATE + 1
SCR_POST_READOUT = SCR_PRE_READOUT + 1
SCR_PRE_CLOSE_SHTR = SCR_POST_READOUT + 1
SCR_POST_CLOSE_SHTR = SCR_PRE_CLOSE_SHTR + 1

# PL_CALLBACK_EVENT
PL_CALLBACK_BOF =  0
PL_CALLBACK_EOF = PL_CALLBACK_BOF + 1
PL_CALLBACK_CHECK_CAMS = PL_CALLBACK_EOF + 1
PL_CALLBACK_CAM_REMOVED = PL_CALLBACK_CHECK_CAMS + 1
PL_CALLBACK_CAM_RESUMED = PL_CALLBACK_CAM_REMOVED + 1
PL_CALLBACK_MAX = PL_CALLBACK_CAM_RESUMED + 1

# PL_MD_FRAME_FLAGS
PL_MD_FRAME_FLAG_ROI_TS_SUPPORTED =  0x01
PL_MD_FRAME_FLAG_UNUSED_2 =  0x02
PL_MD_FRAME_FLAG_UNUSED_3 =  0x04
PL_MD_FRAME_FLAG_UNUSED_4 =  0x10
PL_MD_FRAME_FLAG_UNUSED_5 =  0x20
PL_MD_FRAME_FLAG_UNUSED_6 =  0x40
PL_MD_FRAME_FLAG_UNUSED_7 =  0x80


# PL_MD_ROI_FLAGS
PL_MD_ROI_FLAG_INVALID =  0x01
PL_MD_ROI_FLAG_HEADER_ONLY =  0x02
PL_MD_ROI_FLAG_UNUSED_3 =  0x04
PL_MD_ROI_FLAG_UNUSED_4 =  0x10
PL_MD_ROI_FLAG_UNUSED_5 =  0x20
PL_MD_ROI_FLAG_UNUSED_6 =  0x40
PL_MD_ROI_FLAG_UNUSED_7 =  0x80


# PL_MD_EXT_TAGS
PL_MD_EXT_TAG_PARTICLE_ID =  0
PL_MD_EXT_TAG_PARTICLE_M0 = PL_MD_EXT_TAG_PARTICLE_ID + 1
PL_MD_EXT_TAG_PARTICLE_M2 = PL_MD_EXT_TAG_PARTICLE_M0 + 1
PL_MD_EXT_TAG_MAX = PL_MD_EXT_TAG_PARTICLE_M2 + 1

### STRUCTS ###

class PVCAM_FRAME_INFO_GUID(ctypes.Structure):
    _fields_ = [
        ('f1', ctypes.c_uint32),
        ('f2', ctypes.c_uint16),
        ('f3', ctypes.c_uint16),
        ('f4', ctypes.c_uint8 * 8),
    ]

class FRAME_INFO(ctypes.Structure):
    _fields_ = [
        ('FrameInfoGUID', ctypes.c_void_p),
        ('hCam', ctypes.c_int16),
        ('FrameNr', ctypes.c_int32),
        ('TimeStamp', ctypes.c_long),
        ('ReadoutTime', ctypes.c_int32),
        ('TimeStampBOF', ctypes.c_long),
    ]

class smart_stream_type(ctypes.Structure):
    _fields_ = [
        ('entries', ctypes.c_uint16),
        ('params', ctypes.c_uint32),
    ]

class rgn_type(ctypes.Structure):
    _fields_ = [
        ('s1', ctypes.c_uint16),
        ('s2', ctypes.c_uint16),
        ('sbin', ctypes.c_uint16),
        ('p1', ctypes.c_uint16),
        ('p2', ctypes.c_uint16),
        ('pbin', ctypes.c_uint16),
    ]

class io_entry(ctypes.Structure):
    _fields_ = [
        ('io_port', ctypes.c_uint16),
        ('io_type', ctypes.c_uint32),
        ('state', ctypes.c_float),
    ]

class io_list(ctypes.Structure):
    _fields_ = [
        ('pre_open', ctypes.c_void_p),
        ('post_open', ctypes.c_void_p),
        ('pre_flash', ctypes.c_void_p),
        ('post_flash', ctypes.c_void_p),
        ('pre_integrate', ctypes.c_void_p),
        ('post_integrate', ctypes.c_void_p),
        ('pre_readout', ctypes.c_void_p),
        ('post_readout', ctypes.c_void_p),
        ('pre_close', ctypes.c_void_p),
        ('post_close', ctypes.c_void_p),
    ]

class active_camera_type(ctypes.Structure):
    _fields_ = [
        ('shutter_close_delay', ctypes.c_uint16),
        ('shutter_open_delay', ctypes.c_uint16),
        ('rows', ctypes.c_uint16),
        ('cols', ctypes.c_uint16),
        ('prescan', ctypes.c_uint16),
        ('postscan', ctypes.c_uint16),
        ('premask', ctypes.c_uint16),
        ('postmask', ctypes.c_uint16),
        ('preflash', ctypes.c_uint16),
        ('clear_count', ctypes.c_uint16),
        ('preamp_delay', ctypes.c_uint16),
        ('mpp_selectable', ctypes.c_short),
        ('frame_selectable', ctypes.c_short),
        ('do_clear', ctypes.c_int16),
        ('open_shutter', ctypes.c_int16),
        ('mpp_mode', ctypes.c_short),
        ('frame_transfer', ctypes.c_short),
        ('alt_mode', ctypes.c_short),
        ('exp_res', ctypes.c_uint32),
        ('io_hdr', ctypes.c_void_p),
    ]

class md_frame_header(ctypes.Structure):
    _fields_ = [
        ('signature', ctypes.c_uint32),
        ('version', ctypes.c_uint8),
        ('frameNr', ctypes.c_uint32),
        ('roiCount', ctypes.c_uint16),
        ('timestampBOF', ctypes.c_uint32),
        ('timestampEOF', ctypes.c_uint32),
        ('timestampResNs', ctypes.c_uint32),
        ('exposureTime', ctypes.c_uint32),
        ('exposureTimeResNs', ctypes.c_uint32),
        ('roiTimestampResNs', ctypes.c_uint32),
        ('bitDepth', ctypes.c_uint8),
        ('colorMask', ctypes.c_uint8),
        ('flags', ctypes.c_uint8),
        ('extendedMdSize', ctypes.c_uint16),
        ('imageFormat', ctypes.c_uint8),
        ('imageCompression', ctypes.c_uint8),
        ('_reserved', ctypes.c_uint8 * 6),
    ]

class md_frame_header_v3(ctypes.Structure):
    _fields_ = [
        ('signature', ctypes.c_uint32),
        ('version', ctypes.c_uint8),
        ('frameNr', ctypes.c_uint32),
        ('roiCount', ctypes.c_uint16),
        ('timestampBOF', ctypes.c_void_p),
        ('timestampEOF', ctypes.c_void_p),
        ('exposureTime', ctypes.c_void_p),
        ('bitDepth', ctypes.c_uint8),
        ('colorMask', ctypes.c_uint8),
        ('flags', ctypes.c_uint8),
        ('extendedMdSize', ctypes.c_uint16),
        ('imageFormat', ctypes.c_uint8),
        ('imageCompression', ctypes.c_uint8),
        ('_reserved', ctypes.c_uint8 * 6),
    ]

class md_frame_roi_header(ctypes.Structure):
    _fields_ = [
        ('roiNr', ctypes.c_uint16),
        ('timestampBOR', ctypes.c_uint32),
        ('timestampEOR', ctypes.c_uint32),
        ('roi', ctypes.c_void_p),
        ('flags', ctypes.c_uint8),
        ('extendedMdSize', ctypes.c_uint16),
        ('roiDataSize', ctypes.c_uint32),
        ('_reserved', ctypes.c_uint8 * 3),
    ]

class md_ext_item_info(ctypes.Structure):
    _fields_ = [
        ('tag', ctypes.c_void_p),
        ('type', ctypes.c_uint16),
        ('size', ctypes.c_uint16),
        ('name', ctypes.c_char_p),
    ]

class md_ext_item(ctypes.Structure):
    _fields_ = [
        ('tagInfo', ctypes.c_void_p),
        ('value', ctypes.c_void_p),
    ]

class md_ext_item_collection(ctypes.Structure):
    _fields_ = [
        ('count', ctypes.c_uint16),
    ]

class md_frame_roi(ctypes.Structure):
    _fields_ = [
        ('header', ctypes.c_void_p),
        ('data', ctypes.c_void_p),
        ('dataSize', ctypes.c_uint32),
        ('extMdData', ctypes.c_void_p),
        ('extMdDataSize', ctypes.c_uint16),
    ]

class md_frame(ctypes.Structure):
    _fields_ = [
        ('header', ctypes.c_void_p),
        ('extMdData', ctypes.c_void_p),
        ('extMdDataSize', ctypes.c_uint16),
        ('impliedRoi', ctypes.c_void_p),
        ('roiArray', ctypes.c_void_p),
        ('roiCapacity', ctypes.c_uint16),
        ('roiCount', ctypes.c_uint16),
    ]
