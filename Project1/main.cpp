#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "sndemu/dsp.h"
#include "sndemu/SPC_DSP.h"
#include "sndemu/SPC_Filter.h"
#include <SFML/Audio.hpp>


/*necessary variables*/
typedef unsigned int uint;
uint smppos = 0x200;
SPC_DSP* dsp = new SPC_DSP;
char* aram = (char*)malloc(0x10000);
unsigned int dirposition = 0;


unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};

unsigned char c700sqwave[] = {
    0b10000100, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000011, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77
};

// Constants for the DSP registers.

#define vvoll(n) (n<<4)+0
#define vvolr(n) (n<<4)+1
#define vpl(n) (n<<4)+2
#define vph(n) (n<<4)+3
#define vsrcn(n) (n<<4)+4
#define vadsr1(n) (n<<4)+5
#define vadsr2(n) (n<<4)+6
#define vgain(n) (n<<4)+7
#define len(x) *(&x+1)-x
#define lobit(x) x&0xff
#define hibit(x) x>>8
#define print(x) std::cout << x << std::endl;
#define r(x) dsp->read(x)
#define w(x,y) dsp->write(x,y)


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
    V0GAIN = 0x07,
    KON = 0x4C,
    KOF = 0x5C,
    ADSR = 128,
};




/*functions*/


void pitch(int p, int voice) {
    dsp->write(vpl(voice), p & 0xff);
    dsp->write(vph(voice), (p >> 8)&0x3f);
}


void advance(unsigned int samples) {dsp->run(32*samples);}
void advance() { dsp->run(32); }


//void smp2aram(short* sample, uint length, uint lppoint) {
//    memcpy(aram+smppos, sample, length);
//    aram[0x1000 + dirposition++] = lobit(smppos);
//    aram[0x1000 + dirposition++] = hibit(smppos);
//    aram[0x1000 + dirposition++] = lobit(lppoint+smppos);
//    aram[0x1000 + dirposition++] = hibit(lppoint+smppos);
//    smppos += length;
//}

void tick(unsigned int ticks) {dsp->run(8534 * ticks);}
void tick() { dsp->run(8534); }

/*main*/


int main() {

    /*initialising DSP and registers*/
    if (dsp == NULL) { std::cout << "no DSP lol" << std::endl; }
    dsp->init(aram);
    if (aram == NULL) { std::cout << "no ARAM lol" << std::endl; }

    /*DSP*/
    int sample_count = 32*32000*2;
    spc_dsp_sample_t* output = (spc_dsp_sample_t*)malloc(2 * sample_count * sizeof(spc_dsp_sample_t)); //buffer
    dsp->set_output(output, sample_count);
    if (output == NULL) { print("you are fucking stupid"); return -1; }
    dsp->reset();


    /*load instruments*/

    memcpy(aram + smppos, BRR_SAWTOOTH, len(BRR_SAWTOOTH));
    aram[0x1000 + dirposition++] = lobit(smppos);
    aram[0x1000 + dirposition++] = hibit(smppos);
    aram[0x1000 + dirposition++] = lobit(0 + smppos);
    aram[0x1000 + dirposition++] = hibit(0 + smppos);
    smppos += len(BRR_SAWTOOTH);

    memcpy(aram + smppos, c700sqwave, len(c700sqwave));
    aram[0x1000 + dirposition++] = lobit(smppos);
    aram[0x1000 + dirposition++] = hibit(smppos);
    aram[0x1000 + dirposition++] = lobit(9 + smppos);
    aram[0x1000 + dirposition++] = hibit(9 + smppos);
    smppos += len(c700sqwave);


    /*hopefully creating the DSP sound*/
    w(DIR, 0x10);
    w(MVOLL, 0x80);
    w(MVOLR, 0x80);
    w(vvoll(0), 0x80);
    w(vvolr(0), 0x80);
    w(FLG, 0x20);
    w(vsrcn(0), 0); w(vsrcn(1), 0);
    pitch(0x107f,0); pitch(0xe00, 1);
    w(vadsr1(0), ADSR+0xA);
    w(vadsr2(0), 0xe0);
    w(vadsr1(1), ADSR + 0xA);
    w(vadsr2(1), 0xe0);
    w(KON, 0b00000011);
    dsp->run(32*32000/4 - (256 * 32));
    w(KOF, 1); advance(256); pitch(0x12bf, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000/4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x14ff, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x167f, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x18ff, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x1c40, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x2000, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256); pitch(0x217f, 0); w(KOF, 0); w(KON, 1);
    dsp->run(32 * 32000 / 4 - (256*32));
    w(KOF, 1); advance(256);

    /*debug (remove when not needed)*/
    //int generated_count = dsp->sample_count() / 2;
    //printf("Generated %d samples\n", generated_count);

    //for (int i = 0; i < 10; i++) {
    //    printf("%02d: %04hx %04hx\n", i, output[i * 2], output[1 + i * 2]);
    //}


    /*SFML side of playing sound*/
    sf::SoundBuffer buf(output, 32 * 32000 * 2 + 16, 2, 32000, { sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight });
    sf::SoundBuffer bufwav("its gonna sound like shit trust me bro.wav");
    sf::Sound snd(buf);
    snd.setLooping(0);
    snd.play();
    sf::sleep(sf::milliseconds(2000));

}