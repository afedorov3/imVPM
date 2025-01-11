#include <stdio.h>

    enum StateBits {
        StateExit     = 0,
        StateReady    = 0x08,
        StateIdle     = StateReady,
        // ops has to be LSbs
        StatePlayback = 0x01 | StateReady,
        StateCapture  = 0x02 | StateReady,
        StateRecord   = 0x03 | StateReady,
        StateMask     = StateRecord,
        StateOpMask   = StateMask ^ StateReady,
        StatePause    = 0x10,
        StateSeek     = 0x20,
        StateEOF      = 0x40,
        StateFlagMask = StatePause | StateSeek | StateEOF
    };
    struct State {
        State() : value(StateIdle), hasPlaybackFile(false) {}
        State(unsigned int initial) : value(initial), hasPlaybackFile(false) {}

        // tests
        constexpr bool isReady()     { return  (value & StateReady); }
        constexpr bool isIdle()      { return  (value & StateMask) == StateIdle; }
        constexpr bool isActive()    { return  (value ^ StateMask)  < StateOpMask; }
        constexpr bool isPlaying()   { return  (value & StateMask) == StatePlayback; }
        constexpr bool isCapturing() { return  (value & StateMask) == StateCapture; }
        constexpr bool isRecording() { return  (value & StateMask) == StateRecord; }
        constexpr bool isCapOrRec()  { return  (value & StateCapture) == StateCapture; }
        constexpr bool isPaused()    { return   value & StatePause; }
        constexpr bool atEOF()       { return   value & StateEOF; }

        bool canPlay(bool withFile = false)
                                     { return  (value & ((StateSeek|StateOpMask)^StatePlayback)) == StateIdle && (isPlaying() || withFile || hasPlaybackFile); }
        constexpr bool canCapture()  { return  (value & StateMask) == StateIdle || (value & StateMask) == StateRecord; }
        constexpr bool canRecord()   { return  (value & (StateOpMask^StateCapture)) == StateIdle
                                            || (value & (StatePause|StateMask)) == (StateRecord|StatePause); }
        constexpr bool canPause()    { return  (value & ((StateSeek|StatePause|StateOpMask)^StateCapture)) == StatePlayback; }
        constexpr bool canResume()   { return  (value & ((StateFlagMask|StateOpMask)^StateCapture)) == (StatePlayback|StatePause); }
        constexpr bool canSeek()     { return  (value & (StateSeek|StateMask)) == StatePlayback; }

        // modifiers
        constexpr State &operator=(unsigned int newState) { value = newState; return *this; }
        constexpr State &operator|=(unsigned int set)     { value |= set; return *this; }
        constexpr State &operator&=(unsigned int reset)   { value &= reset; return *this; }

        // access
        constexpr const unsigned int operator()() const { return value; }
        constexpr operator StateBits() const { return (const StateBits)value; }

        unsigned int value;
        bool hasPlaybackFile;
    };

#define STR(...) #__VA_ARGS__
#define TEST(test, expected) do { \
if ((test) != (expected)) \
printf(" " STR(test) ":\t%s expected: %s %s\n", (test) ? "yes" : "no ", (expected) ?  "yes" : "no ", (test) != (expected) ? "  FAIL" : ""); \
} while(0)

void tests(State &state, const bool expected[16])
{
  TEST(state.isReady(),      expected[0]);
  TEST(state.isIdle(),       expected[1]);
  TEST(state.isActive(),     expected[2]);
  TEST(state.isPlaying(),    expected[3]);
  TEST(state.isCapturing(),  expected[4]);
  TEST(state.isRecording(),  expected[5]);
  TEST(state.isCapOrRec(),   expected[6]);
  TEST(state.isPaused(),     expected[7]);
  TEST(state.atEOF(),        expected[8]);
  TEST(state.canPlay(false), expected[9]);
  TEST(state.canPlay(true),  expected[10]);
  TEST(state.canCapture(),   expected[11]);
  TEST(state.canRecord(),    expected[12]);
  TEST(state.canPause(),     expected[13]);
  TEST(state.canResume(),    expected[14]);
  TEST(state.canSeek(),      expected[15]);
}

int main() 
{
  State state;

  printf("0\n");
  state = 0;                            //  RDY   IDL    ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL     cPLF  cCAP    cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false});

  printf("StateIdle\n");
  state = StateIdle;                    //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL   cPLF  cCAP   cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             true, true, false, false, false, false, false, false, false, false, true, true,  true, false, false, false});

  printf("StatePlayback\n");
  state = StatePlayback;                //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL  cPLF  cCAP   cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             true, false, true, true,  false, false, false, false, false, true, true, false, false, true, false, true});

  printf("StatePlayback|StatePause\n");
  state = StatePlayback|StatePause;     //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL  cPLF  cCAP   cREC   cPAU   cRES   cSEE
  tests(state, (const bool[]){             true, false, false, true, false, false, false, true,  false, true, true, false, false, false, true, true});

  printf("StatePlayback|StateEOF\n");
  state = StatePlayback|StateEOF;       //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF   cPL  cPLF  cCAP   cREC   cPAU   cRES   cSEE
  tests(state, (const bool[]){             true, false, false, true, false, false, false, false, true, true, true, false, false, true, false,  true});

  printf("StateCapture\n");
  state = StateCapture;                 //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL   cPLF   cCAP   cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             true, false, true, false,  true, false,  true, false, false, false, false, false, true, false, false, false});

  printf("StateRecord\n");
  state = StateRecord;                  //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL   cPLF   cCAP   cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             true, false, true, false, false, true,   true, false, false, false, false, true,  false, true, false, false});

  printf("StateRecord|StatePause\n");
  state = StateRecord|StatePause;       //  RDY   IDL   ACT    PLY    CAP    REC    CoR    PAU    EOF    cPL   cPLF    cCAP  cREC   cPAU  cRES   cSEE
  tests(state, (const bool[]){             true, false, false, false, false, true,   true, true,  false, false, false, true, true, false, true, false});

  return 0;
}
