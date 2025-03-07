// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

class ClockSetup : public HemisphereApplet {
public:

    enum ClockSetupCursor {
        PLAY_STOP,
        TEMPO,
        SHUFFLE,
        EXT_PPQN,
        MULT1,
        MULT2,
        MULT3,
        MULT4,
        MULT5,
        MULT6,
        MULT7,
        MULT8,
        TRIG1,
        TRIG2,
        TRIG3,
        TRIG4,
        TRIG5,
        TRIG6,
        TRIG7,
        TRIG8,
        LAST_SETTING = TRIG8
    };

    const char* applet_name() {
        return "ClockSet";
    }

    void Start() { }

    // The ClockSetup controller handles MIDI Clock and Transport Start/Stop
    void Controller() {
        bool clock_sync = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_1>();

        // MIDI Clock is filtered to 2 PPQN
        if (frame.MIDIState.clock_q) {
            frame.MIDIState.clock_q = 0;
            clock_sync = 1;
        }
        if (frame.MIDIState.start_q) {
            frame.MIDIState.start_q = 0;
            clock_m->DisableMIDIOut();
            clock_m->Start();
        }
        if (frame.MIDIState.stop_q) {
            frame.MIDIState.stop_q = 0;
            clock_m->Stop();
            clock_m->EnableMIDIOut();
        }

        // Paused means wait for clock-sync to start
        if (clock_m->IsPaused() && clock_sync)
            clock_m->Start();
        // TODO: automatically stop...

        // Advance internal clock, sync to external clock / reset
        if (clock_m->IsRunning())
            clock_m->SyncTrig( clock_sync );

        // ------------ //
        if (clock_m->IsRunning() && clock_m->MIDITock()) usbMIDI.sendRealTime(usbMIDI.Clock);

        // 8 internal clock flashers
        for (int i = 0; i < 8; ++i) {
            if (clock_m->Tock(i))
                flash_ticker[i] = HEMISPHERE_PULSE_ANIMATION_TIME;
            else if (flash_ticker[i])
                --flash_ticker[i];
        }

        if (button_ticker) --button_ticker;
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (!EditMode()) { // special cases for toggle buttons
            if (cursor == PLAY_STOP) PlayStop();
            else CursorAction(cursor, LAST_SETTING);
        }
        else CursorAction(cursor, LAST_SETTING);

        if (cursor == TEMPO) {
            // Tap Tempo detection
            if (last_tap_tick) {
                tap_time[taps] = OC::CORE::ticks - last_tap_tick;

                if (tap_time[taps] > CLOCK_TICKS_MAX) {
                    taps = 0;
                    last_tap_tick = 0;
                }
                else if (++taps == NR_OF_TAPS)
                    clock_m->SetTempoFromTaps(tap_time, taps);

                taps %= NR_OF_TAPS;
            }
            last_tap_tick = OC::CORE::ticks;
        }
    }

    void OnEncoderMove(int direction) {
        taps = 0;
        last_tap_tick = 0;
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        switch ((ClockSetupCursor)cursor) {
        case PLAY_STOP:
            PlayStop();
            break;

        case TRIG1:
        case TRIG2:
        case TRIG3:
        case TRIG4:
        case TRIG5:
        case TRIG6:
        case TRIG7:
        case TRIG8:
            HS::trigger_mapping[cursor-TRIG1] = constrain( HS::trigger_mapping[cursor-TRIG1] + direction, 0, 8);
            break;

        /* the boops shall return in a hidden form
        case BOOP1:
        case BOOP2:
        case BOOP3:
        case BOOP4:
            clock_m->Boop(cursor-BOOP1);
            button_ticker = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
            break;
        */

        case EXT_PPQN:
            clock_m->SetClockPPQN(clock_m->GetClockPPQN() + direction);
            break;
        case TEMPO:
            clock_m->SetTempoBPM(clock_m->GetTempo() + direction);
            break;
        case SHUFFLE:
            clock_m->SetShuffle(clock_m->GetShuffle() + direction);
            break;

        case MULT1:
        case MULT2:
        case MULT3:
        case MULT4:
        case MULT5:
        case MULT6:
        case MULT7:
        case MULT8:
            clock_m->SetMultiply(clock_m->GetMultiply(cursor - MULT1) + direction, cursor - MULT1);
            break;

        default: break;
        }
    }

    // Same data blobs as T3 version, but different layout
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 9 }, clock_m->GetTempo());
        Pack(data, PackLocation { 9, 7 }, clock_m->GetShuffle());
        for (size_t i = 0; i < 8; ++i) {
            Pack(data, PackLocation { 16+i*6, 6 }, clock_m->GetMultiply(i)+32);
        }

        return data;
    }
    void OnDataReceive(uint64_t data) {
        if (!clock_m->IsRunning())
            clock_m->SetTempoBPM(Unpack(data, PackLocation { 0, 9 }));
        clock_m->SetShuffle(Unpack(data, PackLocation { 9, 7 }));
        for (size_t i = 0; i < 8; ++i) {
            clock_m->SetMultiply(Unpack(data, PackLocation { 16+i*6, 6 })-32, i);
        }
    }

    uint64_t GetGlobals() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 1 }, HS::auto_save_enabled);
        Pack(data, PackLocation { 1, 1 }, HS::cursor_wrap);
        Pack(data, PackLocation { 2, 2 }, HS::screensaver_mode);
        Pack(data, PackLocation { 4, 7 }, HS::trig_length);
        Pack(data, PackLocation { 11, 5 }, clock_m->GetClockPPQN());
        // 48 bits free
        return data;
    }
    void SetGlobals(const uint64_t &data) {
        HS::auto_save_enabled = Unpack(data, PackLocation { 0, 1 });
        HS::cursor_wrap = Unpack(data, PackLocation { 1, 1 });
        HS::screensaver_mode = Unpack(data, PackLocation { 2, 2 });
        HS::trig_length = constrain( Unpack(data, PackLocation { 4, 7 }), 1, 127);
        clock_m->SetClockPPQN(Unpack(data, PackLocation { 11, 5 }));
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "";
        help[HEMISPHERE_HELP_CVS]      = "";
        help[HEMISPHERE_HELP_OUTS]     = "";
        help[HEMISPHERE_HELP_ENCODER]  = "";
        //                               "------------------" <-- Size Guide
    }

private:
    int cursor; // ClockSetupCursor
    int flash_ticker[8];
    int button_ticker;
    ClockManager *clock_m = clock_m->get();

    static const int NR_OF_TAPS = 3;

    int taps = 0; // tap tempo
    uint32_t tap_time[NR_OF_TAPS]; // buffer of past tap tempo measurements
    uint32_t last_tap_tick = 0;

    void PlayStop() {
        if (clock_m->IsRunning()) {
            clock_m->Stop();
        } else {
            bool p = clock_m->IsPaused();
            clock_m->Start( !p ); // stop->pause->start
        }
    }

    void DrawInterface() {
        // Header: This is sort of a faux applet, so its header
        // needs to extend across the screen
        graphics.setPrintPos(1, 2);
        graphics.print("Clocks/Triggers");
        gfxLine(0, 10, 127, 10);

        int y = 14;
        // Clock State
        gfxIcon(1, y, CLOCK_ICON);
        if (clock_m->IsRunning()) {
            gfxIcon(12, y, PLAY_ICON);
        } else if (clock_m->IsPaused()) {
            gfxIcon(12, y, PAUSE_ICON);
        } else {
            gfxIcon(12, y, STOP_ICON);
        }

        // Tempo
        gfxPrint(22 + pad(100, clock_m->GetTempo()), y, clock_m->GetTempo());
        if (cursor != SHUFFLE)
            gfxPrint(" BPM");
        else {
            // Shuffle
            gfxIcon(44, y, METRO_R_ICON);
            gfxPrint(52 + pad(10, clock_m->GetShuffle()), y, clock_m->GetShuffle());
            gfxPrint("%");
        }

        // Input PPQN
        gfxPrint(79, y, "Sync=");
        gfxPrint(clock_m->GetClockPPQN());

        y += 10;
        for (int ch=0; ch<8; ++ch) {
            const int x = (ch % 4) * 32;
            if (ch == 4) y += 10;

            // Multipliers
            int mult = clock_m->GetMultiply(ch);
            if (0 != mult || cursor == MULT1 + ch) { // hide if 0
                gfxPrint(1 + x, y, (mult >= 0) ? "x" : "/");
                gfxPrint( (mult >= 0) ? mult : 1 - mult );
            }

            // Physical trigger input mappings
            gfxPrint(1 + x, y + 21, OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch] ] );

            // Trigger indicators
            gfxIcon(23 + x, y + 20, DOWN_BTN_ICON);
            if (flash_ticker[ch]) gfxInvert(22 + x, y + 22, 9, 8);
        }

        y += 9;
        gfxDottedLine(0, y, 127, y, 3);

        switch ((ClockSetupCursor)cursor) {
        case PLAY_STOP:
            gfxFrame(11, 13, 10, 10);
            break;
        case TEMPO:
            gfxCursor(22, 22, 19);
            break;
        case SHUFFLE:
            gfxCursor(52, 22, 13);
            break;
        case EXT_PPQN:
            gfxCursor(109,22, 13);
            break;

        case MULT1:
        case MULT2:
        case MULT3:
        case MULT4:
        case MULT5:
        case MULT6:
        case MULT7:
        case MULT8:
        {
          const int x_ = 8 + 32 * ((cursor-MULT1) % 4);
          const int y_ = 32 + ((cursor-MULT1) / 4 * 10);
          gfxCursor(x_, y_, 12);
          break;
        }

        case TRIG1:
        case TRIG2:
        case TRIG3:
        case TRIG4:
        case TRIG5:
        case TRIG6:
        case TRIG7:
        case TRIG8:
        {
          const int x_ = 1 + 32 * ((cursor-TRIG1) % 4);
          const int y_ = 53 + ((cursor-TRIG1) / 4 * 10);
          gfxCursor(x_, y_, 19);
          break;
        }

        /* the boops shall return in a hidden form
        case BOOP1:
        case BOOP2:
        case BOOP3:
        case BOOP4:
            if (0 == button_ticker)
                gfxIcon(12 + 32*(cursor-BOOP1), 49, LEFT_ICON);
            break;
        */

        default: break;
        }
    }
};

ClockSetup ClockSetup_instance;
