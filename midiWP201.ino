//JT - AO  artilect fablab
//vibrato -> tension filtre
//velocity * volume -> VCA
//note-> VCO, 5 octaves moins les 5 premieres et les 5 dernieres notes

#include <SoftwareSerial.h>
//#define ADSRauto  //si on supprime cette ligne, on ne fait que gate
#define DBG
/***********définitions hard *************/
#define gate 5
#define grossecaisse 6  // et sortie de programmation
#define caisseclaire 7  //  et ADSRauto
#define HH 8
#define tom1 10   //déplacé pour garder sortie timer2A
#define OH A0
#define tom2 A1
#define tom3 A2
#define cymbale A3
#define ngrossecaisse 36  // notes correspondantes
#define ncaisseclaire 38
#define nHH 42
#define ntom1 43
#define nOH 46
#define ntom2 47
#define ntom3 48
#define ncymbale 49
#define dureepulse 50    // duree impulsion de commande des percus
#define pente 1000// a regler pour compenser la valeur inexacte des 5V
#define offset 0  //pour recadrer le bas de la gamme
#define notebase 24
/********************/
int octave=0; //depart en do 0, donc gamme Mi 0 à La 3
bool contolchange = true;   //VCF sur vibrato, sinon sur aftertouch (pkp)?
bool attaque= true;  //ADSRauto validé
bool debutADSR = false;
bool messagedispo=false;
byte commandByte, commandBytetmp;
byte noteByte;
byte velocityByte;
bool etatnote = false;
bool moitie=false;
bool moitie1=false;
bool moitie2=false;
bool troisbytes=false; // pour messages à 3 octets
bool premier=true;
bool action=false;
bool sortie=false;
bool ADSRtype= false;
int sor;
int seqADSR; // sequence ADSR
// definitions midi
const byte noteON =   0x90; //canal 1
const byte noteOFF =  0x80; //canal 1
const byte control =  0xB0; //canal 1 , message 176, type, valeur
const byte pitchBend = 0xF0; //canal 1 , message 240,0,E2p
const byte reverb =  0x5B; // E1p  91
const byte tremolo = 0x5C; // E2n  92
const byte vibrato = 0x4D; // E3   77
const byte chorus =  0x5D; // E1n   93
const byte timbre =  0x47; // E4    71
const byte volu =  0x07;
const byte pkp =     0xA0; //canal 1 polyphonic key pressure
const byte progchge= 0xC0; //canal 1 programme change 2 bytes seulement
const byte chanpres = 0xD0; //canal 1 channel pressure 2 bytes seulement
const byte pbchange = 0xE0; // canal 1 pitch bend toutes notes du canal
const byte SysComMes= 0xF0; // system common message ignorer tout ce qui suit

int lfo1, lfo2, lfo3;  //dents de scie pour vibrato, tremolo etc..
long time, oldtime, oldtime2;  //pour faire LFO
long time1, time2, time3, time4, time5, time6, time7, time8, timeADSR;
unsigned int duree,tension_note,tension_amplitude, tension_amplitude2,tension_filtre,tension_tmp, tmp_note;
byte velocity,vtremolo,vvibrato, vpitchbend,vreverb, vchorus, vvolume, canal,runstat; 
int note = 0, vol = 0 , amplitude, amplidrum; //amplitude est velocity*vvolume


SoftwareSerial midiSerial(12, 13); //pins 12,13 pour Rx 12 et TX 13 

//***************************************************//
void setup() {
  Serial.begin(9600);
  midiSerial.begin(31250);
  pinMode(gate, OUTPUT); //gate as output
  pinMode(grossecaisse, INPUT_PULLUP);
  digitalWrite(grossecaisse,1);
  pinMode(caisseclaire, INPUT_PULLUP);
  digitalWrite(caisseclaire,1);
  pinMode(HH, INPUT_PULLUP);
  digitalWrite(HH,1);
  pinMode(tom1, INPUT_PULLUP);
  digitalWrite(tom1,1); 
  pinMode(OH, INPUT_PULLUP); 
  pinMode(tom2, INPUT_PULLUP);
  pinMode(tom3, INPUT_PULLUP);
  pinMode(cymbale, INPUT_PULLUP);
  //programmation pendant 5 sec max. fini par5 sec ou grossecaisse
  oldtime=millis();
  while (((millis()-oldtime<= 5000))&&sortie==false)
  {
    sor=digitalRead(grossecaisse);
    if(sor==0) sortie=true;
    sor=digitalRead(caisseclaire);
    if(sor==0)
    {ADSRtype= true;  //ADSRauto
    oldtime=millis(); //on repart pour 5 secondes
    delay(500);
    digitalWrite(caisseclaire,1);
    }
    sor=digitalRead(HH);
    if(sor==0)
    {octave+=1;  //debut en do0 et ++
    if(octave>=3) octave=3;
    oldtime=millis(); //on repart pour 5 secondes
    delay(500);
    digitalWrite(HH,1);
    }
    sor=digitalRead(tom1);
    if(sor==0)
    {
     octave-=1;  //debut en do1 et --
     if(octave<=0) octave=0;
     oldtime=millis(); //on repart pour 5 secondes
     delay(500);
     digitalWrite(tom1,1);
     }
  }
    Serial.println( "sorti  ");
    if (ADSRtype)  Serial.println( "ADSR automatique  "); 
    else  Serial.println( "ADSR calculé  ");
    Serial.println( "octave  " + (String)octave);
    delay(1000);
    pinMode(grossecaisse, OUTPUT);
    pinMode(caisseclaire, OUTPUT);
    pinMode(HH, OUTPUT);
    pinMode(tom1, OUTPUT); 
    pinMode(OH, OUTPUT); 
    pinMode(tom2, OUTPUT);
    pinMode(tom3, OUTPUT);
    pinMode(cymbale, OUTPUT);
    digitalWrite(grossecaisse,0);
    digitalWrite(caisseclaire,0);
    digitalWrite(HH, 0);
    digitalWrite(tom1,0); 
    digitalWrite(OH, 0); 
    digitalWrite(tom2, 0);
    digitalWrite(tom3,0);
    digitalWrite(cymbale,0);
    vvolume=110;
    digitalWrite(gate,0);
    oldtime=millis();
    seqADSR=0;
    timeADSR=oldtime;
    messagedispo=false;
    tension_filtre=120;  //arbitraire
    vpitchbend=64;
    vvibrato=0;
    vtremolo=0;
    InitAudioPWM();
    Serial.println("init effectuée  ");
}
//**********************************//
void loop() 
{ 
  LFO();
  checkMIDI();
  if (messagedispo)
  {
    messagedispo=false;
    if  (commandByte >127)
    { 
     runstat=commandByte;
     switch(commandByte&0xF0)  
     {
      case pkp:       //aftertouch poly
       troisbytes= true;
       premier=true;
       break;
      case progchge:  //programm change
       troisbytes= false;
       premier=true;
       break; 
      case pbchange:  //pitch bend
       troisbytes=true;
       premier=true;
       break;
      case chanpres:  //aftertouch mono
       troisbytes=false;
       premier=true;
       break; 
      case SysComMes: //sysrem common
       troisbytes=false;
       premier=true;
       break; 
      case noteON:
       canal=runstat&0x0F;
#ifdef DBG
       Serial.print("MIDI noteON canal=   ");Serial.println(canal+1);
#endif
       troisbytes=true;
       premier=true;
       break; 
      case noteOFF:
       canal=runstat&0x0F;
#ifdef DBG       
       Serial.print("MIDI noteOFF canal=   ");Serial.println(canal+1);
#endif       
       troisbytes=true;
       premier=true;
       break; 
      case control:     //control change
       canal=runstat&0x0F;
       troisbytes=true;
       premier=true;
      default:  
       break;  
    }
   }
   else
   {
    //il s'agit de données   
    switch(runstat&0xF0) //status de la derniere commande
    {
      case pkp:
       premier=false;
       break;
      case progchge:
       premier=false;
       break; 
      case pbchange:
       premier=false;
       break;
      case chanpres:
       premier=false;
       break; 
      case SysComMes:
       premier=false;
       break; 
      case noteON:
       if(premier) 
       {
        note=commandByte;
        premier=false;
       }
       else
       {
        velocity=commandByte;
        premier=true;
        action=true;
       }
       break; 
      case noteOFF:
       if(premier) 
       {
        note=commandByte;
        premier=false;
       }
       else
       {
        velocity=commandByte;
        premier=true;
        action=true;
       }
       break; 
      case control:
       if(premier) 
       {
        note=commandByte;
        premier=false;
       }
       else
       {
        velocity=commandByte;
        premier=true;
        action=true;
       }
       break; 
      default:  
      break;        
    }//fin switch runstat
   }// fin données
  }
//suite boucle
/*****************************/
  if(action)
  {
    switch(runstat&0xF0)
    {
      case noteON:
        if (canal !=9)  //drum traité à part
        {
         if(velocity==0)
         {
          #ifdef ADSRauto
          digitalWrite(gate,0);
          vpitchbend=64;
          #else
          seqADSR=10;
          #endif
         }
         else
         {
          digitalWrite(gate,1); 
          tmp_note=((map(note, (notebase+12*octave), (notebase+12*octave)+60, 0, 1023))*pente/1000)+offset+((vpitchbend-64)/2)+((lfo1*vvibrato)/64);
          tension_note=constrain(tmp_note, 60, 950);
          amplitude=(velocity*vvolume)+(lfo2*vtremolo);
          tension_amplitude=constrain(map(amplitude , 0, 16256, 0, 255), 0, 255);
#ifdef DBG          
          Serial.println("tension = " +(String)tension_note +"pour note = " + (String)note);
          Serial.println("tension = " +(String)tension_amplitude +"pour velocity = " + (String)velocity+"et volume = " +(String)vvolume);
#endif          
          debutADSR=true;
          #ifdef ADSRauto
          debutADSR=false;
          #endif
         }
       }
       else     // canal batterie
      {
       if(velocity !=0)
        {
        amplidrum= velocity*vvolume;
        //Serial.println("lancer drum " + String(amplidrum/128));//lancer impulsions drums 
         switch(note)  
         {
          case ngrossecaisse:
           digitalWrite(grossecaisse,1);
           time1=millis();
#ifdef DBG
           Serial.println(" grosse caisse " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case ncaisseclaire:
           digitalWrite(caisseclaire,1);
           time2=millis();
#ifdef DBG
           Serial.println(" caisse claire " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case nHH:
           digitalWrite(HH,1);
           time3=millis();
#ifdef DBG
           Serial.println(" charley " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case ntom1:
           digitalWrite(tom1,1);
           time4=millis();
#ifdef DBG           
           Serial.println(" tom 1 " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break; 
          case nOH:
           digitalWrite(OH,1);
           time5=millis();
#ifdef DBG           
           Serial.println(" charley ouverte " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case ntom2:
           digitalWrite(tom2,1);
           time6=millis();
#ifdef DBG           
           Serial.println(" tom 2 " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case ntom3:
           digitalWrite(tom3,1);
           time7=millis();
#ifdef DBG           
           Serial.println(" tom 3 " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          case ncymbale:
           digitalWrite(cymbale,1);
           time8=millis();
#ifdef DBG           
           Serial.println(" cymbale " + String(amplidrum/128));//lancer impulsions drums 
#endif           
           break;
          default:
           break;
         }
        }
      }
       break; 
      case noteOFF:
       if(canal!=9)
       {
        #ifdef ADSRauto
        digitalWrite(gate,0);
        vpitchbend=64;
        #else
        seqADSR=10;
        #endif
       }
       break; 
      case control:
       switch(note)
       {
        case pitchBend:
           vpitchbend=velocity;
         break;
        case vibrato:
          vvibrato=velocity;
        //  tension_filtre=2*vvibrato;
         break;
        case tremolo:
           vtremolo=velocity;       
         break;
        case reverb:
           vreverb=velocity;
         break;
        case chorus:
         vchorus=velocity;
         break;
        case volu:
           vvolume=velocity;
         break;
        default:
         break; 
        }
       break; 
      default:  
       break;        
    }//fin switch runstat
    action=false;
  }

if (!ADSRtype)
{
if (debutADSR)
 {
 seqADSR=1;
 debutADSR=false; 
 timeADSR=millis();
 }
switch(seqADSR)
 {
 case 0:
  break;
 case 1:
  tension_amplitude2= tension_amplitude/3; 
  seqADSR=2;
  timeADSR=millis();
#ifdef DBG
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude2));
#endif 
  break;
 case 2:
  if ((millis()-timeADSR)>=15)
   {
    tension_amplitude2= tension_amplitude*3/5;
   seqADSR=3;  
   }
#ifdef DBG 
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude2));
#endif  
  break;
 case 3:
  if ((millis()-timeADSR)>=30)
   {
     tension_amplitude2= tension_amplitude;
    seqADSR=4; 
   }
#ifdef DBG
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude));
#endif    
  break;
 case 4:
  if ((millis()-timeADSR)>=40)
   {
     tension_amplitude2=tension_amplitude+(tension_amplitude/8);
    seqADSR=5; 
   }
#ifdef DBG 
   Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude+(tension_amplitude/8)));
#endif     
   break;
 case 5:
  if ((millis()-timeADSR)>=50)
   {
     tension_amplitude2= tension_amplitude;    
    seqADSR=0;
   }
#ifdef DBG  
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude2));
#endif    
  break;
 case 10:  // debut de release
  timeADSR=millis();
  tension_amplitude2= tension_amplitude*0.9;
  seqADSR=11;
#ifdef DBG 
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude2));
#endif    
  break;
 case 11:
  if ((millis()-timeADSR)>=10)
  {
    tension_amplitude2= tension_amplitude*0.81;
    seqADSR=12;    
  }
  break;
 case 12:
  if ((millis()-timeADSR)>=20)
  {
    tension_amplitude2= tension_amplitude*0.72;
    seqADSR=13;    
  }
#ifdef DBG
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude2));
#endif    
  break;
 case 13:
  if ((millis()-timeADSR)>=30)
  {
    tension_amplitude2= tension_amplitude*0.63;
   seqADSR=14;    
  }
  break;  
 case 14:
  if ((millis()-timeADSR)>=40)
  {
    tension_amplitude2= tension_amplitude*0.56;
   seqADSR=15;    
  }
  break;  
 case 15:
  if ((millis()-timeADSR)>=50)
  {
    tension_amplitude2= tension_amplitude*0.45;
   seqADSR=16;    
  }
#ifdef DBG
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude*0.45));
#endif  
  break;  
 case 16:
  if ((millis()-timeADSR)>=60)
  {
    tension_amplitude2= tension_amplitude*0.36;
    seqADSR=17;    
  }
  break;  
 case 17:
  if ((millis()-timeADSR)>=70)
  {
    tension_amplitude2= tension_amplitude*0.27;
    seqADSR=18;    
  }
  break;  
 case 18:
  if ((millis()-timeADSR)>=80)
  {
   tension_amplitude2= tension_amplitude*0.18;
   seqADSR=19;    
  }
#ifdef DBG 
  Serial.println("ADSR seq " + (String)seqADSR + " tension = " +(String)(tension_amplitude*0.18));
#endif  
  break;
 case 19:
  if ((millis()-timeADSR)>=90)
  {
   tension_amplitude2= tension_amplitude*0.09;
   seqADSR=20;    
  }
  break;  
 case 20:
  if ((millis()-timeADSR)>=100)
  {
    tension_amplitude2= 0;
   seqADSR=0;  
   digitalWrite(gate, 0);   //raz gate  
   vpitchbend=64;
  }
  break;
 default:
  break;
 }
}
else   tension_amplitude2= tension_amplitude;  //if ADSRauto
time=millis();
if (time-time1>=dureepulse) digitalWrite(grossecaisse,0);
if (time-time2>=dureepulse) digitalWrite(caisseclaire,0);
if (time-time3>=dureepulse) digitalWrite(HH,0);
if (time-time4>=dureepulse) digitalWrite(tom1,0);
if (time-time5>=dureepulse) digitalWrite(OH,0);
if (time-time6>=dureepulse) digitalWrite(tom2,0);
if (time-time7>=dureepulse) digitalWrite(tom3,0);
if (time-time8>=dureepulse) digitalWrite(cymbale,0);

}

//********************************************/
void checkMIDI()
{
     if (midiSerial.available()>0) 
     { 
       commandByte = midiSerial.read();//read first byte
       //Serial.print("MIDI reçu   ");Serial.println( commandByte);
       messagedispo=true;  
     }

}  

void LFO()
{
time=millis();
duree=time-oldtime;
if (duree>= 8)
  {
    oldtime=time;
    if (moitie1)
    {
      lfo1 +=(duree*7)/10;
      if (lfo1>=127)
      {
        lfo1=127;
         moitie1=false;
      }
    }
    else
    {
      lfo1 -=(duree*7)/10;
      if (lfo1<=0)
      {
        lfo1=0;
        moitie1=true;
      }
    }
  }
  if (moitie2)
    {
      lfo2 +=duree;
      if (lfo2>=127)
      {
        lfo2=127;
         moitie2=false;
      }
    }
    else
    {
      lfo2 -=duree;
      if (lfo2<=0)
      {
        lfo2=0;
        moitie2=true;
      }
    }
    if (moitie)
    {
      //lfo3 +=2*duree;
      lfo3 +=duree/4;
      if (lfo3>=127)
      {
        lfo3=127;
         moitie=false;
      }
    }
    else
    {
      lfo3 -=duree/4;
      //lfo3 -=2*duree;
      if (lfo3<=0)
      {
        lfo3=0;
        moitie=true;
      }
    }
}

void InitAudioPWM(void)
{
    InitTimer2();   
    InitTimer1();   
    TCCR2B |= _BV(CS20); //start timer 2 horloge directe
    TIMSK2 |= _BV(TOIE2); //demarrage des IT 
    TCCR1B |= _BV(CS10); //start timer 1 horloge directe
    TIMSK1 |= _BV(TOIE1); //demarrage des IT 
}
static void InitTimer2(void)
{
    TIMSK2 = 0x00;  //do not use interrupts
    TCCR2B = 0x00;  //no input clock, timer stopped
    TCNT2 = 0x00;     //fast PWM, pas de prescaler,
    TCCR2A = _BV(COM2A1) |  _BV(WGM21) |  _BV(WGM20) | _BV(COM2B1);      //compare outputs OC2A, non-inverting mode
    TCCR1C = 0x00;
    OCR2A = 150;  // milieu de l'échelle
    OCR2B= 50;
    DDRB |= (_BV(DDB3));  // pin 11 en sortie pour OC2A (PB3)
    DDRD |= _BV(DDD3);  // pin 3 en sortie pour OC2A (PD3)
}
ISR(TIMER2_OVF_vect) //remplace les analogwrite
{ 
   //OCR2A = sample; faire calcul pour le prochain coup
   OCR2A = tension_amplitude2;
   OCR2B = tension_filtre;
}
static void InitTimer1(void)
{
   TIMSK1 = 0x00;  //do not use interrupts
   TCCR1B = 0x00;  //no input clock, timer stopped
   TCNT1 = 0x00;  //fast PWM, pas de prescaler, 10 bits
   TCCR1A = _BV(COM1A1) |  _BV(WGM11) |  _BV(WGM10); //compare outputs OC2A, non-inverting mode
   OCR1A = 500;  // milieu de l'échelle
   DDRB |= (_BV(DDB1));  // pin 9 en sortie pour OC1A (PB1)
}
ISR(TIMER1_OVF_vect)
{
 OCR1A= tension_note; 
}
