// Hosyond 4.0" battery: electrical sampling and voltage curve are ported
// from mjwgeek/SquachWatch-CYD-4.0/src/main.cpp CYD40 implementation.
// The animated lava lamp follows its src/ui_clear.cpp indicator style.
#pragma once
#ifdef HOSYOND_CYD40
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

namespace HosyondBattery {
struct Reading {
  uint16_t mv;
  uint8_t percent;
  bool present;
  bool chargingEstimated;
};
static Reading current = {0,0,false,false};
static uint32_t lastSample = 0;
static uint16_t voltsHistory[30] = {0};
static uint8_t historyN = 0, historyAt = 0;
static const uint16_t MV[] = {3300,3500,3600,3700,3750,3800,3850,3900,4000,4100,4200};
static const uint8_t PCT[] = {0,5,10,20,30,40,50,60,75,90,100};

inline uint16_t readMilliVolts() {
  uint32_t sum = 0;
  for (uint8_t i=0;i<8;i++) sum += analogReadMilliVolts(34);
  return (uint16_t)((sum / 8) * 2);  // CYD40's 2:1 pack-voltage divider
}
inline uint8_t mapPercent(uint16_t mv) {
  if (mv <= MV[0]) return 0;
  for (uint8_t i=1;i<sizeof(MV)/sizeof(MV[0]);i++) {
    if (mv<=MV[i])
      return (uint8_t)(PCT[i-1] +
          (uint32_t)(PCT[i]-PCT[i-1])*(mv-MV[i-1])/(MV[i]-MV[i-1]));
  }
  return 100;
}
inline const Reading& sample() {
  uint32_t now=millis();
  if (lastSample && now-lastSample<2000) return current;
  lastSample=now ? now:1;
  current.mv=readMilliVolts();
  current.present=current.mv>=2500;
  if (!current.present) {
    current.percent=0; current.chargingEstimated=false; historyN=historyAt=0;
    return current;
  }
  current.percent=mapPercent(current.mv);
  voltsHistory[historyAt]=current.mv;
  historyAt=(uint8_t)((historyAt+1)%30);
  if(historyN<30)historyN++;
  uint16_t low=UINT16_MAX, high=0;
  for(uint8_t i=0;i<historyN;i++){
    if(voltsHistory[i]<low) low=voltsHistory[i];
    if(voltsHistory[i]>high)high=voltsHistory[i];
  }
  // SquachWatch's heuristic: 40mV rise from recent minimum suggests USB
  // current; 40mV drop from recent peak suggests disconnection.
  // No CHRG pin is exposed, so this is *not* confirmed charge state.
  if(!current.chargingEstimated && current.mv>=low+40)
    current.chargingEstimated=true;
  else if(current.chargingEstimated && current.mv+40<=high)
    current.chargingEstimated=false;
  return current;
}
inline void format(char* buf,size_t len) {
  const Reading& r=sample();
  if(!r.present) snprintf(buf,len,"NONE");
  else snprintf(buf,len,"%u%% %u.%02uV",(unsigned)r.percent,
                (unsigned)(r.mv/1000),(unsigned)(r.mv%1000/10));
}
inline uint16_t color(uint8_t r,uint8_t g,uint8_t b) {
  return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));
}
inline uint16_t mix(uint16_t a,uint16_t b,uint8_t weight) {
  uint16_t inv=255-weight;
  uint8_t r=(uint8_t)((((a>>11)&31)*inv+((b>>11)&31)*weight)/255);
  uint8_t g=(uint8_t)((((a>>5)&63)*inv+((b>>5)&63)*weight)/255);
  uint8_t bl=(uint8_t)(((a&31)*inv+(b&31)*weight)/255);
  return (r<<11)|(g<<5)|bl;
}
inline uint16_t rainbow(uint16_t degrees) {
  uint8_t r=0,g=0,b=0;
  degrees%=360;
  uint8_t f=(uint8_t)((degrees%60)*255/60);
  switch(degrees/60) {
    case 0:r=255;g=f;break;
    case 1:r=255-f;g=255;break;
    case 2:g=255;b=f;break;
    case 3:g=255-f;b=255;break;
    case 4:r=f;b=255;break;
    default:r=255;b=255-f;break;
  }
  return color(r,g,b);
}
inline float random01(uint32_t a,uint32_t b) {
  uint32_t h=(a*374761393u+b*668265263u)^0x5bd1e995u;
  h=(h^(h>>13))*1274126177u;
  return (float)(h^(h>>16))/4294967295.0f;
}
// A compact (13 x 21-cell) version of SquachWatch's no-backplate,
// outline-glow lamp, with pink/purple/cyan liquid, shimmer, and bubbles.
inline void drawLavaLamp(TFT_eSPI& t,int ox,int oy,int scale,uint32_t now) {
  const Reading& r=sample();
  if(!r.present) return;
  constexpr int GW=13, GH=21;
  uint16_t pixels[GH][GW] = {};
  uint8_t occupied[GH][GW] = {};
  auto put=[&](int x,int y,uint16_t c) {
    if(x< -1||x>11||y< -1||y>19)return;
    occupied[y+1][x+1]=1; pixels[y+1][x+1]=c;
  };
  auto fill=[&](int x,int y,int w,int h,uint16_t c) {
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)put(x+i,y+j,c);
  };
  auto glass=[](int y,int& a,int& z) {
    float hw=2.0f+(float)(y-3)/11.0f*2.5f;
    a=(int)lroundf(5.0f-hw);z=(int)lroundf(5.0f+hw);
  };
  bool low=r.percent<=10 && !r.chargingEstimated;
  int n=(int)lroundf(12.0f*r.percent/100.0f);
  const uint16_t purple=color(164,82,237), pink=color(255,100,204),
                 cyan=color(87,233,251);
  float k=(sinf((float)now/1800.0f)+1.0f)*0.5f;
  uint16_t hue=k<0.5f ? mix(pink,purple,(uint8_t)(k*510.0f))
                       : mix(purple,cyan,(uint8_t)((k-0.5f)*510.0f));
  uint16_t liq=hue;
  if(low && random01(1,now/90)>0.55f)liq=mix(hue,TFT_BLACK,179);
  if(r.chargingEstimated)
    liq=mix(hue,TFT_WHITE,(uint8_t)(38+38*sinf((float)now/150.0f)));
  uint16_t liqEdge=mix(liq,TFT_BLACK,102);
  auto metalRow=[&](int y,bool dark)->uint16_t {
    uint16_t v=r.chargingEstimated?rainbow((uint16_t)((now/6+y*24)%360)):purple;
    return dark?mix(v,TFT_BLACK,90):v;
  };
  fill(4,0,3,1,metalRow(0,false));
  fill(3,1,5,1,metalRow(1,false));
  fill(3,2,5,1,metalRow(2,true));
  for(int y=3;y<=14;y++){
    int a,z;glass(y,a,z);
    bool isFilled=14-y<n;
    for(int x=a;x<=z;x++){
      bool edge=x==a||x==z;
      put(x,y,isFilled?(edge?liqEdge:liq):(edge?0x51ED:0x2065));
    }
  }
  if(n>1){
    float sp=r.chargingEstimated?2.5f:1.0f;
    const float phase[3]={0.0f,1.7f,3.1f};
    for(int i=0;i<3;i++){
      float ph=(float)now*sp/900.0f+phase[i];
      int yy=14-(int)lroundf((sinf(ph)+1.0f)*0.5f*(float)(n-1));
      int a,z;glass(yy,a,z);
      int xx=(int)lroundf(5.0f+sinf(phase[i]*3.0f+i)*(float)(z-a)/4.0f);
      put(xx,yy,i==1?cyan:color(255,235,110));
      put(xx+1,yy,i==1?cyan:color(255,235,110));
    }
  }
  if(r.chargingEstimated && n>=2){
    for(int i=0;i<3;i++){
      uint32_t phase=(now+i*233u)%700u;
      int yy=14-(int)(phase*(uint32_t)n/700u);
      if(yy>14-n)put(4+i,yy,mix(hue,TFT_WHITE,200));
    }
  }
  fill(2,15,7,1,metalRow(15,true));
  fill(2,16,7,1,metalRow(16,false));
  fill(2,17,7,1,metalRow(17,false));
  fill(1,18,9,1,metalRow(18,false));
  if(r.percent>=100 && (now/300)%2==0)put(5,0,TFT_WHITE);
  const uint16_t glow=mix(hue,TFT_BLACK, r.chargingEstimated
     ?(uint8_t)(60+110*(0.5f+0.5f*sinf((float)now/160.0f))):154);
  for(int gy=0;gy<GH;gy++)for(int gx=0;gx<GW;gx++){
    if(occupied[gy][gx])continue;
    bool near=false;
    for(int dy=-1;dy<=1&&!near;dy++)for(int dx=-1;dx<=1;dx++){
      int ny=gy+dy,nx=gx+dx;
      if(ny>=0&&ny<GH&&nx>=0&&nx<GW&&occupied[ny][nx]==1)near=true;
    }
    if(near) {occupied[gy][gx]=2;pixels[gy][gx]=(gy>=4&&gy<=15&&14-(gy-1)<n)?glow:TFT_BLACK;}
  }
  for(int gy=0;gy<GH;gy++){
    int gx=0;
    while(gx<GW){
      if(!occupied[gy][gx]){gx++;continue;}
      uint16_t c=pixels[gy][gx];int run=1;
      while(gx+run<GW&&occupied[gy][gx+run]&&pixels[gy][gx+run]==c)run++;
      t.fillRect(ox+(gx-1)*scale,oy+(gy-1)*scale,run*scale,scale,c);
      gx+=run;
    }
  }
}
} // namespace HosyondBattery
#endif
