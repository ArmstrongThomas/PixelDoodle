/* Glyphs adapted from the public 3DS UI renderer. */
static void glyphRows(char c, unsigned char rows[7])
{
    memset(rows, 0, 7);
    switch (c)
    {
        case '0': { const unsigned char g[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; memcpy(rows,g,7); break; }
        case '1': { const unsigned char g[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; memcpy(rows,g,7); break; }
        case '2': { const unsigned char g[7]={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; memcpy(rows,g,7); break; }
        case '3': { const unsigned char g[7]={0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}; memcpy(rows,g,7); break; }
        case '4': { const unsigned char g[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; memcpy(rows,g,7); break; }
        case '5': { const unsigned char g[7]={0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; memcpy(rows,g,7); break; }
        case '6': { const unsigned char g[7]={0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}; memcpy(rows,g,7); break; }
        case '7': { const unsigned char g[7]={0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; memcpy(rows,g,7); break; }
        case '8': { const unsigned char g[7]={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; memcpy(rows,g,7); break; }
        case '9': { const unsigned char g[7]={0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}; memcpy(rows,g,7); break; }
        case 'A': { const unsigned char g[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'B': { const unsigned char g[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; memcpy(rows,g,7); break; }
        case 'C': { const unsigned char g[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; memcpy(rows,g,7); break; }
        case 'D': { const unsigned char g[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; memcpy(rows,g,7); break; }
        case 'E': { const unsigned char g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; memcpy(rows,g,7); break; }
        case 'F': { const unsigned char g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; memcpy(rows,g,7); break; }
        case 'G': { const unsigned char g[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; memcpy(rows,g,7); break; }
        case 'H': { const unsigned char g[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'I': { const unsigned char g[7]={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}; memcpy(rows,g,7); break; }
        case 'J': { const unsigned char g[7]={0x07,0x02,0x02,0x02,0x12,0x12,0x0C}; memcpy(rows,g,7); break; }
        case 'K': { const unsigned char g[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11}; memcpy(rows,g,7); break; }
        case 'L': { const unsigned char g[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; memcpy(rows,g,7); break; }
        case 'M': { const unsigned char g[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'N': { const unsigned char g[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'O': { const unsigned char g[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(rows,g,7); break; }
        case 'P': { const unsigned char g[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; memcpy(rows,g,7); break; }
        case 'Q': { const unsigned char g[7]={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; memcpy(rows,g,7); break; }
        case 'R': { const unsigned char g[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; memcpy(rows,g,7); break; }
        case 'S': { const unsigned char g[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; memcpy(rows,g,7); break; }
        case 'T': { const unsigned char g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(rows,g,7); break; }
        case 'U': { const unsigned char g[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(rows,g,7); break; }
        case 'V': { const unsigned char g[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; memcpy(rows,g,7); break; }
        case 'W': { const unsigned char g[7]={0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; memcpy(rows,g,7); break; }
        case 'X': { const unsigned char g[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'Y': { const unsigned char g[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; memcpy(rows,g,7); break; }
        case 'Z': { const unsigned char g[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; memcpy(rows,g,7); break; }
        case 'a': { const unsigned char g[7]={0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}; memcpy(rows,g,7); break; }
        case 'b': { const unsigned char g[7]={0x10,0x10,0x16,0x19,0x11,0x11,0x1E}; memcpy(rows,g,7); break; }
        case 'c': { const unsigned char g[7]={0x00,0x00,0x0E,0x10,0x10,0x11,0x0E}; memcpy(rows,g,7); break; }
        case 'd': { const unsigned char g[7]={0x01,0x01,0x0D,0x13,0x11,0x11,0x0F}; memcpy(rows,g,7); break; }
        case 'e': { const unsigned char g[7]={0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}; memcpy(rows,g,7); break; }
        case 'f': { const unsigned char g[7]={0x06,0x08,0x08,0x1C,0x08,0x08,0x08}; memcpy(rows,g,7); break; }
        case 'g': { const unsigned char g[7]={0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E}; memcpy(rows,g,7); break; }
        case 'h': { const unsigned char g[7]={0x10,0x10,0x16,0x19,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'i': { const unsigned char g[7]={0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}; memcpy(rows,g,7); break; }
        case 'j': { const unsigned char g[7]={0x02,0x00,0x06,0x02,0x02,0x12,0x0C}; memcpy(rows,g,7); break; }
        case 'k': { const unsigned char g[7]={0x10,0x10,0x12,0x14,0x18,0x14,0x12}; memcpy(rows,g,7); break; }
        case 'l': { const unsigned char g[7]={0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}; memcpy(rows,g,7); break; }
        case 'm': { const unsigned char g[7]={0x00,0x00,0x1A,0x15,0x15,0x15,0x15}; memcpy(rows,g,7); break; }
        case 'n': { const unsigned char g[7]={0x00,0x00,0x16,0x19,0x11,0x11,0x11}; memcpy(rows,g,7); break; }
        case 'o': { const unsigned char g[7]={0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}; memcpy(rows,g,7); break; }
        case 'p': { const unsigned char g[7]={0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}; memcpy(rows,g,7); break; }
        case 'q': { const unsigned char g[7]={0x00,0x00,0x0D,0x13,0x0F,0x01,0x01}; memcpy(rows,g,7); break; }
        case 'r': { const unsigned char g[7]={0x00,0x00,0x16,0x19,0x10,0x10,0x10}; memcpy(rows,g,7); break; }
        case 's': { const unsigned char g[7]={0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E}; memcpy(rows,g,7); break; }
        case 't': { const unsigned char g[7]={0x08,0x08,0x1C,0x08,0x08,0x09,0x06}; memcpy(rows,g,7); break; }
        case 'u': { const unsigned char g[7]={0x00,0x00,0x11,0x11,0x11,0x13,0x0D}; memcpy(rows,g,7); break; }
        case 'v': { const unsigned char g[7]={0x00,0x00,0x11,0x11,0x11,0x0A,0x04}; memcpy(rows,g,7); break; }
        case 'w': { const unsigned char g[7]={0x00,0x00,0x11,0x15,0x15,0x15,0x0A}; memcpy(rows,g,7); break; }
        case 'x': { const unsigned char g[7]={0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}; memcpy(rows,g,7); break; }
        case 'y': { const unsigned char g[7]={0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}; memcpy(rows,g,7); break; }
        case 'z': { const unsigned char g[7]={0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}; memcpy(rows,g,7); break; }
        case '-': { const unsigned char g[7]={0,0,0,0x1F,0,0,0}; memcpy(rows,g,7); break; }
        case ':': { const unsigned char g[7]={0,0x04,0,0,0x04,0,0}; memcpy(rows,g,7); break; }
        case '.': { const unsigned char g[7]={0,0,0,0,0,0x0C,0x0C}; memcpy(rows,g,7); break; }
        case '/': { const unsigned char g[7]={0x01,0x01,0x02,0x04,0x08,0x10,0x10}; memcpy(rows,g,7); break; }
        case '<': { const unsigned char g[7]={0x02,0x04,0x08,0x10,0x08,0x04,0x02}; memcpy(rows,g,7); break; }
        case '>': { const unsigned char g[7]={0x08,0x04,0x02,0x01,0x02,0x04,0x08}; memcpy(rows,g,7); break; }
        case '[': { const unsigned char g[7]={0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; memcpy(rows,g,7); break; }
        case ']': { const unsigned char g[7]={0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; memcpy(rows,g,7); break; }
        case '_': { const unsigned char g[7]={0,0,0,0,0,0,0x1F}; memcpy(rows,g,7); break; }
        case '!': { const unsigned char g[7]={0x04,0x04,0x04,0x04,0x04,0,0x04}; memcpy(rows,g,7); break; }
        case '?': { const unsigned char g[7]={0x0E,0x11,0x01,0x02,0x04,0,0x04}; memcpy(rows,g,7); break; }
        case ',': { const unsigned char g[7]={0,0,0,0,0,0x04,0x08}; memcpy(rows,g,7); break; }
        case '\'': { const unsigned char g[7]={0x04,0x04,0x08,0,0,0,0}; memcpy(rows,g,7); break; }
        case '"': { const unsigned char g[7]={0x0A,0x0A,0x14,0,0,0,0}; memcpy(rows,g,7); break; }
        case '#': { const unsigned char g[7]={0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0}; memcpy(rows,g,7); break; }
        case '%': { const unsigned char g[7]={0x19,0x19,0x02,0x04,0x08,0x13,0x13}; memcpy(rows,g,7); break; }
        case '&': { const unsigned char g[7]={0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}; memcpy(rows,g,7); break; }
        case '(': { const unsigned char g[7]={0x02,0x04,0x08,0x08,0x08,0x04,0x02}; memcpy(rows,g,7); break; }
        case ')': { const unsigned char g[7]={0x08,0x04,0x02,0x02,0x02,0x04,0x08}; memcpy(rows,g,7); break; }
        case '*': { const unsigned char g[7]={0,0x15,0x0E,0x1F,0x0E,0x15,0}; memcpy(rows,g,7); break; }
        case '+': { const unsigned char g[7]={0,0x04,0x04,0x1F,0x04,0x04,0}; memcpy(rows,g,7); break; }
        case '=': { const unsigned char g[7]={0,0x1F,0,0x1F,0,0,0}; memcpy(rows,g,7); break; }
        case '@': { const unsigned char g[7]={0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}; memcpy(rows,g,7); break; }
        case '|': { const unsigned char g[7]={0x04,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(rows,g,7); break; }
        case ';': { const unsigned char g[7]={0,4,0,0,4,4,8}; memcpy(rows,g,7); break; }
        case '$': { const unsigned char g[7]={4,15,20,14,5,30,4}; memcpy(rows,g,7); break; }
        case '\\': { const unsigned char g[7]={16,16,8,4,2,1,1}; memcpy(rows,g,7); break; }
        case '^': { const unsigned char g[7]={4,10,17,0,0,0,0}; memcpy(rows,g,7); break; }
        case '`': { const unsigned char g[7]={8,4,2,0,0,0,0}; memcpy(rows,g,7); break; }
        case '{': { const unsigned char g[7]={2,4,4,8,4,4,2}; memcpy(rows,g,7); break; }
        case '}': { const unsigned char g[7]={8,4,4,2,4,4,8}; memcpy(rows,g,7); break; }
        case '~': { const unsigned char g[7]={0,0,9,22,0,0,0}; memcpy(rows,g,7); break; }
        case ' ': break;
        default: { const unsigned char g[7]={0x1F,0x11,0x15,0x15,0x11,0x11,0x1F}; memcpy(rows,g,7); break; }
    }
}

/*
 * Six antialiased 16x16 UI glyphs derived from the mockup icon shapes.
 * Four-bit coverage reuses the unreachable lowercase font cells, adding 768
 * bytes instead of embedding another typeface.
 */
static const unsigned short uiIconAlpha[6][4][16]={
    /* undo-2 */
    {
    {0,0,0,0,0,0,0,0,0,2400,0,0,0,40208,0,0},
    {9,53760,0,0,190,39048,34914,0,190,39048,35006,24576,9,53760,3,58112},
    {0,40208,0,31232,0,2400,0,19712,0,0,0,23552,0,0,1,54784},
    {0,3,13149,40960,0,45,56774,0,0,0,0,0,0,0,0,0},
    },
    /* redo-2 */
    {
    {0,0,0,0,0,0,0,0,0,0,1680,0,0,0,729,0},
    {0,0,29,36864,0,9864,34953,60160,6,60296,34953,60160,62,12288,29,36864},
    {167,0,729,0,196,0,1680,0,182,0,0,0,109,4096,0,0},
    {10,54579,12288,0,0,27869,53760,0,0,0,0,0,0,0,0,0},
    },
    /* palette */
    {
    {0,19,12544,0,0,19454,61382,0,9,59152,365,45312,156,4096,17409,44032},
    {1249,6976,47872,3184,2928,6976,4352,1232,7952,0,27,16626,15874,28672,27,16611},
    {15879,57856,0,992,7952,8192,0,15728,2928,0,15582,59136,1249,0,50994,0},
    {156,4096,43008,0,9,59152,23552,0,0,19454,59136,0,0,19,8192,0},
    },
    /* layers-3 */
    {
    {0,2,8192,0,0,1215,64320,0,5,52865,6108,24576,1757,24576,6,56672},
    {12112,0,0,1522,3048,4096,1,36528,58,59952,670,46080,5632,10731,48786,97},
    {7843,22,28928,15073,654,46080,75,59680,513,32454,27879,4128,11856,108,50688,1506},
    {1757,24576,6,56672,5,52865,6108,24576,0,1215,64320,0,0,2,8192,0},
    },
    /* toolbox */
    {
    {0,51,13056,0,0,3053,57008,0,0,15872,227,0,2,40584,35049,8192},
    {62,43912,35002,58112,998,0,0,28224,7776,0,0,1761,15616,5632,97,211},
    {15923,28211,13286,13283,24541,57309,56829,56821,15616,15616,211,211,15616,512,32,211},
    {15616,0,0,211,7832,34952,34952,35297,632,34952,34952,34592,0,0,0,0},
    },
    /* menu */
    {
    {0,0,0,0,0,0,0,0,35,13107,13107,12800,173,56797,56797,55808},
    {0,0,0,0,0,0,0,0,0,0,0,0,104,34952,34952,34304},
    {104,34952,34952,34304,0,0,0,0,0,0,0,0,0,0,0,0},
    {173,56797,56797,55808,35,13107,13107,12800,0,0,0,0,0,0,0,0},
    },
};

static const unsigned short uiLayerIconAlpha[6][4][16]={
    /* grip-vertical */
    {
    {0,0,0,0,0,0,0,0,0,2720,2720,0,0,12274,12274,0},
    {0,1360,1360,0,0,0,0,0,0,272,272,0,0,7905,7905,0},
    {0,7905,7905,0,0,272,272,0,0,0,0,0,0,1360,1360,0},
    {0,12274,12274,0,0,2720,2720,0,0,0,0,0,0,0,0,0},
    },
    /* eye */
    {
    {0,0,0,0,0,0,0,0,0,19,12544,0,0,23806,61381,0},
    {27,54800,365,45312,458,4119,28929,44048,2480,490,44560,2960,7712,1952,2672,737},
    {7712,1952,2928,737,2480,490,48656,2960,458,4119,28929,44048,27,54800,365,45312},
    {0,23806,61381,0,0,19,12544,0,0,0,0,0,0,0,0,0},
    },
    /* eye-off */
    {
    {512,0,0,0,11856,0,0,0,1253,3,12544,0,78,24654,61381,0},
    {28,62720,365,45312,458,24144,1,44048,2480,1525,0,2960,7712,1998,20480,737},
    {7712,1956,58624,993,2224,490,53072,2944,458,4119,30181,1296,27,54800,415,24576},
    {0,23806,61384,58624,0,19,12544,20048,0,0,0,1506,0,0,0,32},
    },
    /* lock-open */
    {
    {0,2,8192,0,0,1246,60736,0,0,15968,1763,0,0,43008,138,0},
    {0,54272,21,0,0,54016,0,0,19,58931,13107,12544,749,60893,56797,56864},
    {2192,0,0,2432,2176,0,0,2176,2176,0,0,2176,2176,0,0,2176},
    {2176,0,0,2176,2192,0,0,2688,749,56797,56797,56864,19,13107,13107,12544},
    },
    /* lock-keyhole */
    {
    {0,2,8192,0,0,1246,60736,0,0,15968,1762,0,0,43008,138,0},
    {0,54272,77,0,0,54016,61,0,413,64989,56799,55568,1987,17203,13108,15456},
    {2176,0,0,2176,2176,5,20480,2176,2176,47,61952,2176,2176,10,40960,2176},
    {2176,0,0,2176,2192,0,0,2688,749,56797,56797,56864,19,13107,13107,12544},
    },
    /* sliders-horizontal */
    {
    {0,0,0,0,0,0,5632,0,307,13104,15923,13072,1757,56786,24541,56672},
    {0,0,15616,0,0,0,512,0,0,10752,0,0,904,44547,34952,34864},
    {904,44547,34952,34864,0,10752,0,0,0,0,32,0,0,0,211,0},
    {1757,56797,24813,56672,307,13107,4310,13072,0,0,97,0,0,0,0,0},
    },
};

