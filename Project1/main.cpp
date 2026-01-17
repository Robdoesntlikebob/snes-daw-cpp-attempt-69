#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "sndemu/dsp.h"
#include "sndemu/SPC_DSP.h"
#include "sndemu/SPC_Filter.h"
#include <SFML/Audio.hpp>

unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x7f,
};

short c700sqwave[] = {
    0b10000100, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000011, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77
};

// Constants for the DSP registers.

#define vvoll(n) (0x##n<<4)+0
#define vvolr(n) (0x##n<<4)+1
#define vpl(n) (0x##n<<4)+2
#define vph(n) (0x##n<<4)+3
#define vsrcn(n) (0x##n<<4)+4
#define vadsr1(n) (0x##n<<4)+5
#define vadsr2(n) (0x##n<<4)+6
#define len(x) *(&x+1)-x

enum {
    MVOLL = 0x0C,
    MVOLR = 0x1C,
    FLG = 0x6C,
    DIR = 0x5D,
    V0VOLL = 0x00,
    V0VOLR = 0x01,
    V0PITCHL = 0x02,
    V0PITCHH = 0x03,
    V0SRCN = 0x04,
    V0ADSR1 = 0x05,
    V0ADSR2 = 0x06,
    KON = 0x4C,
};

SPC_DSP* dsp;
SPC_Filter* f;

void load() {
    dsp = new SPC_DSP;
    f = new SPC_Filter;
}

void pitch(int p, int voice) {
    dsp->write(V0PITCHL, p&0xff);
    dsp->write(V0PITCHH, (p >> 8)&0x3f);
}

int main(int argc, char* argv[]) {
    load();
    if (dsp == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate DSP");
        return 1;
    }

    // Allocate some RAM for it.
    unsigned char aram[0x10000]; // 64KB
    dsp->write(DIR, 0x01);

    if (aram == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate ARAM");
        return 1;
    }

    // Connect the ARAM to the DSP.
    spc_dsp_init(dsp, aram);

    // Allocate an output buffer for audio samples.
    // The DSP generates stereo samples at 32kHz,
    // let's reserve room for a second of audio.
    int sample_count = 32*32000*2;
    spc_dsp_sample_t* output =
        (spc_dsp_sample_t*)malloc(2 * sample_count * sizeof(spc_dsp_sample_t));

    if (output == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate output buffer");
        return 1;
    }

    // Connect the output buffer to the DSP.
    spc_dsp_set_output(dsp, output, sample_count);

    // Now everything's connected, turn it on.
    spc_dsp_reset(dsp);

    // Load our instrument into the beginning of ARAM.
    memcpy(aram+0x200, BRR_SAWTOOTH, sizeof(BRR_SAWTOOTH) / sizeof(unsigned char));


    // There's only one entry in our table.
    aram[0x1000] = 0x00; // sample start address low byte
    aram[0x1000+1] = 0x02; // sample start address high byte
    aram[0x1000+2] = 0x09; // sample loop address low byte
    aram[0x1000+3] = 0x02; // sample loop address high byte

    // Tell the DSP that the table of samples starts at 0x0100
    spc_dsp_write(dsp, DIR, 0x10);

    // Tell the DSP to set the channel volume to max.
    spc_dsp_write(dsp, V0VOLL, 0x0);
    spc_dsp_write(dsp, V0VOLR, 0x0);

    // Tell the DSP to set the master volume to max.
    spc_dsp_write(dsp, MVOLL, 0x80);
    spc_dsp_write(dsp, MVOLR, 0x80);

    // Tell the DSP to unmute audio output.
    spc_dsp_write(dsp, FLG, 0x20);

    // Tell the DSP that voice 0 should play sample 0.
    spc_dsp_write(dsp, V0SRCN, 0);

    // Tell the DSP to play the sample at the original pitch.
    pitch(0x107f, 0);

    // Set up an ADSR envelope for this voice.
    // We don't want anything complicated,
    // just start at max volume and stay there until release.
    spc_dsp_write(dsp, V0ADSR1, 0b10011001);
    spc_dsp_write(dsp, V0ADSR2, 0b10101110);

    // Tell the DSP to activate voice 0.
    dsp->run(32);
    spc_dsp_write(dsp, KON, 0x01);
    spc_dsp_write(dsp, V0VOLL, 0x80);
    spc_dsp_write(dsp, V0VOLR, 0x80);

    // Every 32 clocks, a two-channel sample is written to the output buffer,
    // so if we run for 320 clocks, we should get 10 two-channel samples.
    // The DSP only checks for KON every other sample,
    // after KON it spends five samples doing setup work,
    // our instrument is 16 samples long,
    // and the DSP outputs a sample every 32 clocks.
    // So to see a full loop of our instrument:
    spc_dsp_run(dsp, 32 * 32000 * 2);
    f->run(output, 32 * 32000 * 2);

    // How many samples did we actually get?
    // Note that the library counts a two-channel sample as "two samples".
    // man i fucking hate comments
    int generated_count = spc_dsp_sample_count(dsp) / 2;
    printf("Generated %d samples\n", generated_count);

    //// Display the samples the DSP produced
    //for (int i = 0; i < generated_count; i++) {
    //    printf("%02d: %04hx %04hx\n", i, output[i * 2], output[1 + i * 2]);
    //}
    //printf("\n");
    std::cout << "V4SRCN = " << vvoll(4) << std::endl;

    sf::SoundBuffer buf(output, 32 * 32000 * 2, 2, 32000, {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight});
    sf::SoundBuffer bufwav("its gonna sound like shit trust me bro.wav");
    sf::Sound snd(buf);
    snd.setLooping(0);
    snd.play();
    sf::sleep(sf::milliseconds(2000));

    // We're all done!
    return 0;
}