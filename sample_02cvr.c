#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* 2012-1-30 */
/* N_(x)<=6 */

/* 2021-1-12 ２次元化 */
/* 2021-1-21 属性名辞書追加 */
/* 2021-1-25 変量X、Yの標準化対応版 */
/* 2023-5-09 軽微な修正 */

/* Files */
#define DATANAME "testdata1s.txt" /* input (data name) */
#define DICNAME "testdic1.txt" /* input (attribute name) */
/* (95+2) x (***) */

#define fpoola "zrp01a.txt" /* output (rule pool a) (attibute name) */
#define fpoolb "zrp01b.txt" /* output (rule pool b) (number) */
#define fcont "zrd01.txt" /* document file */
#define RESULTNAME "zrmemo01.txt" /* メモ用ファイル*/

/* Data */
#define Nrd 159 /* number of total records */
#define Nzk 95 /* number of attributes for antecedent */

/* Settings for Rules */
#define Nrulemax 2002 /* max rule in the pool */
#define Minsup 0.04 /* minimum support */
#define Maxsigx 0.5 /* maximum sigma for x */
#define Maxsigy 0.5 /* maximum sigma for y */

/* Settings for rule mining */
#define Nstart 1000 /* start of file ID */
#define Ntry 100 /* Number of cycles */

/* GNP */
#define Generation 201 /* Final Generation */
#define Nkotai 120 /* pop size */
#define Npn 10 /* number of processing nodes (individual) */
#define Njg 100/* number of judgement nodes (individual) */
#define Nmx 7 /* max number of Yes-side transitions +1 (Max 9) */
#define Muratep 1 /* mutation rate-p */
#define Muratej 6 /* mutation1 rate */
#define Muratea 6 /* mutation2 rate */
#define Nkousa 20 /* frequency of crossover（each individual）*/

void main()
{
struct asrule{
	int zen0;
	int zen1;
	int zen2;
	int zen3;
	int zen4;
	int zen5;
	int zen6;
	int zen7;
	double nx_mean;
	double nx_sigma;
	double ny_mean;
	double ny_sigma;
	int n_anter;
	int n_nvalue;
	int mh;
	int mj;	
	};
struct asrule kekka[Nrulemax];

struct cmrule
{
	int zen0;
	int zen1;
	int zen2;
	int zen3;
	int zen4;
	int zen5;
	int zen6;
	int zen7;
};
struct cmrule cmprule[Nrulemax];

int a[5],b,rulenum,rulenum2,rulenuh,rulenuj;
int i,j,k,i1,j1,k1,i2,j2,k2,i3,j3,k3,i4,j4,k4,i5,j5,i6,j6,i7,j7,k7,k8,ii;
int nodenum;
int ks,ng,nk;
int mnant,pid;
int nflag,sflag1,sflag2,mdflag,murand,muatt1,muatt2,muatt3,trkall,ndif;
int kousap,kousaj1,kousaj2,kmemo,smemo,nodememo,nodes1,nodes2;
int bc2,memoh,memoj;
double bx,by,imx,isx,imy,isy;

int totnum,newnotm,totnuh,totnuj,newnoth,newnotj;
int fi,fj,fk,fi1,gj;
int fk1,fk2,fk3,fk4,fk5;
int gk1,gk2,gk3,gk4,gk5;

double confmax,fitaveall,supp,sigmadx,sigmady;
double zokux,zokuy;

static int node[Nkotai][Npn+Njg][2];

static int zoku[Nzk+2];

static int kazu[Nkotai][Npn][10];
static int nva[Nkotai][Npn][10];
static int mva[Nkotai][Npn][10];
static int zname[Nkotai][Npn][10];

static double nxwa[Nkotai][Npn][10];
static double nxsigma[Nkotai][Npn][10];
static double nywa[Nkotai][Npn][10];
static double nysigma[Nkotai][Npn][10];

int nextrl[Ntry];

int trkcan[5][Nzk];
int att[Nzk];
int trk[Nzk];
double fitness[Nkotai];
int fitrank[Nkotai],neoflag[Nkotai];

static int genecnct[Nkotai][Npn+Njg];
static int geneatt[Nkotai][Npn+Njg];

int attm[10],aa[10],noberule[10];

int attset[Nzk+1];

int asrulepre[10];
int asrulecan[10];
int asruleme[10];

int newnot[Nrulemax];

char frul[12];
char frap[12];
char frnt[12];
char floc[12];

char items[31];
char ditem[Nzk+3][31];

FILE *fdata1;
FILE *fdata2;
/*
FILE *fdata3;
*/
FILE *fdata4;/* DICNAME */
FILE *fdata5;
FILE *fdata6;
FILE *fdata7;
/*
FILE *fdata8;
*/
FILE *fdata9;
/*
FILE *fdata0;
*/
FILE *fdatara;/* fpool a */
FILE *fdatarb;/* fpool b */
FILE *fdatac;/* fcont */

srand(1);/* rundomize */

for(ii=0;ii<Nzk+3;ii++)
{
	strcpy(ditem[ii],"\0");
}

fdata4=fopen(DICNAME,"r");
for(ii=0;ii<Nzk+3;ii++)
{
	fscanf(fdata4,"%d%s",&pid,items);
	strcpy(ditem[ii],items);
}/* for ii */
fclose(fdata4);

totnum=0;
totnuh=0;
totnuj=0;

fdatac=fopen(fcont,"w");
fprintf(fdatac,"\n");
fclose(fdatac);

for(fj=Nstart;fj<(Nstart+Ntry);fj++)
{

for(i=0;i<Nrulemax;i++)
{
	kekka[i].zen0=0;
	kekka[i].zen1=0;
	kekka[i].zen2=0;
	kekka[i].zen3=0;
	kekka[i].zen4=0;
	kekka[i].zen5=0;
	kekka[i].zen6=0;
	kekka[i].zen7=0;
	kekka[i].nx_mean=0;
	kekka[i].nx_sigma=0;
	kekka[i].ny_mean=0;
	kekka[i].ny_sigma=0;
	kekka[i].n_anter=0;
	kekka[i].n_nvalue=0;
	kekka[i].mh=0;
	kekka[i].mj=0;
}

for(i=0;i<Nrulemax;i++)
{
	cmprule[i].zen0=0;
	cmprule[i].zen1=0;
	cmprule[i].zen2=0;
	cmprule[i].zen3=0;
	cmprule[i].zen4=0;
	cmprule[i].zen5=0;
	cmprule[i].zen6=0;
	cmprule[i].zen7=0;
}

for(i=0;i<Nrulemax;i++)
{
newnot[i]=0;
}

/* save new file */
 fk5=fj/10000;
 fk4=fj/1000-fk5*10;
 fk3=fj/100-fk5*100-fk4*10;
 fk2=fj/10-fk5*1000-fk4*100-fk3*10;
 fk1=fj-fk5*10000-fk4*1000-fk3*100-fk2*10;

	frul[0]=73;
	frul[1]=76;
	frul[2]=48+fk5;
	frul[3]=48+fk4;
	frul[4]=48+fk3;
	frul[5]=48+fk2;
	frul[6]=48+fk1;
	frul[7]=46;
	frul[8]=116;
	frul[9]=120;
	frul[10]=116;
	frul[11]=0;

	frap[0]=73;
	frap[1]=65;
	frap[2]=48+fk5;
	frap[3]=48+fk4;
	frap[4]=48+fk3;
	frap[5]=48+fk2;
	frap[6]=48+fk1;
	frap[7]=46;
	frap[8]=116;
	frap[9]=120;
	frap[10]=116;
	frap[11]=0;

	frnt[0]=73;
	frnt[1]=84;
	frnt[2]=48+fk5;
	frnt[3]=48+fk4;
	frnt[4]=48+fk3;
	frnt[5]=48+fk2;
	frnt[6]=48+fk1;
	frnt[7]=46;
	frnt[8]=116;
	frnt[9]=120;
	frnt[10]=116;
	frnt[11]=0;

	floc[0]=73;
	floc[1]=66;
	floc[2]=48+fk5;
	floc[3]=48+fk4;
	floc[4]=48+fk3;
	floc[5]=48+fk2;
	floc[6]=48+fk1;
	floc[7]=46;
	floc[8]=116;
	floc[9]=120;
	floc[10]=116;
	floc[11]=0;

for(fi=0;fi<Nzk+1;fi++)
{
	attset[fi]=fi;
}

rulenum=1;
rulenuh=1;
rulenuj=1;

for(i=0;i<10;i++)
{
	noberule[i]=0;
}

trkall=Nzk;

for(i=0;i<Nzk;i++)
{
	att[i]=0;
	trk[i]=0;
	trkcan[0][i]=1;
	trkcan[1][i]=0;
	trkcan[2][i]=0;
	trkcan[3][i]=0;
	trkcan[4][i]=0;
}

for(i4=0;i4<Nkotai;i4++)
{
	for(j4=0;j4<(Npn+Njg);j4++)
	{
		genecnct[i4][j4]=rand()%Njg+Npn;
		geneatt[i4][j4]=rand()%Nzk;
	}
}

fdata5=fopen(frul,"w");
fprintf(fdata5,"");
fclose(fdata5);

fdata6=fopen(frap,"w");
fprintf(fdata6,"\n");
fclose(fdata6);

fdata9=fopen(floc,"w");
fprintf(fdata9,"\n");
fclose(fdata9);

/* ***** main ***** */

for(ng=0;ng<Generation;ng++)
{
	if(rulenum>(Nrulemax-2))
	{
		break;
	}

/* initialize GNP genes */
for(nk=0;nk<Nkotai;nk++)
{
	for(j4=0;j4<(Npn+Njg);j4++)
	{
		node[nk][j4][0]=geneatt[nk][j4];
		node[nk][j4][1]=genecnct[nk][j4];
	}
}	
/* initialize of setting */

for(nk=0;nk<Nkotai;nk++)
{
	fitness[nk]=(double)nk*(-0.00001);
	fitrank[nk]=0;
	
	for(k=0;k<Npn;k++)
	{
		for(i=0;i<10;i++)
		{
			kazu[nk][k][i]=0;
			zname[nk][k][i]=0;
			nva[nk][k][i]=0;
			mva[nk][k][i]=0;
			nxwa[nk][k][i]=0;
			nxsigma[nk][k][i]=0;
			nywa[nk][k][i]=0;
			nysigma[nk][k][i]=0;

		}
	}
}

/* examination */
fdata2=fopen(DATANAME,"r");

for(i=0;i<Nrd;i++)
{
	for(j7=0;j7<Nzk;j7++)
	{
		fscanf(fdata2,"%d",&b);
		zoku[j7]=b;
	}
	
		fscanf(fdata2,"%lf%lf",&bx,&by);
		zokux=bx;
		zokuy=by;

	for(nk=0;nk<Nkotai;nk++)
	{
		for(k=0;k<Npn;k++)
		{
			nodenum=k;
			j=0;
			mdflag=1;
			kazu[nk][k][j]=kazu[nk][k][j]+1;
			mva[nk][k][j]=mva[nk][k][j]+1;
			nodenum=node[nk][nodenum][1];
	
			while(nodenum>(Npn-1) && j<Nmx)
			{
				j=j+1;
				zname[nk][k][j]=node[nk][nodenum][0]+1;
				if(zoku[node[nk][nodenum][0]]==1)
				{
					if(mdflag==1)
					{
						kazu[nk][k][j]=kazu[nk][k][j]+1;
						nxwa[nk][k][j]=nxwa[nk][k][j]+zokux;
						nxsigma[nk][k][j]=nxsigma[nk][k][j]+zokux*zokux;
						nywa[nk][k][j]=nywa[nk][k][j]+zokuy;
						nysigma[nk][k][j]=nysigma[nk][k][j]+zokuy*zokuy;
					}
					mva[nk][k][j]=mva[nk][k][j]+1;
					nodenum=node[nk][nodenum][1];
				}
				else if(zoku[node[nk][nodenum][0]]==0)
				{
					nodenum=k;
				}
				else
				{
					mva[nk][k][j]=mva[nk][k][j]+1;
					mdflag=0;
					nodenum=node[nk][nodenum][1];
				}
			}
		}/* k */
	}/* nk */
}/* i */
fclose(fdata2);

for(nk=0;nk<Nkotai;nk++)
{
	for(k=0;k<Npn;k++)
	{
		for(i=0;i<Nmx;i++)
		{
			nva[nk][k][i]=kazu[nk][k][0]-mva[nk][k][i]+kazu[nk][k][i];
		}
	}
}

/* pre processing */

for(nk=0;nk<Nkotai;nk++)
{
	for(k=0;k<Npn;k++)
	{
		for(j6=1;j6<9;j6++)
		{
			if(kazu[nk][k][j6]!=0)
			{
				nxwa[nk][k][j6]=nxwa[nk][k][j6]/(double)kazu[nk][k][j6];
				nxsigma[nk][k][j6]=nxsigma[nk][k][j6]/(double)kazu[nk][k][j6]-nxwa[nk][k][j6]*nxwa[nk][k][j6];
				nywa[nk][k][j6]=nywa[nk][k][j6]/(double)kazu[nk][k][j6];
				nysigma[nk][k][j6]=nysigma[nk][k][j6]/(double)kazu[nk][k][j6]-nywa[nk][k][j6]*nywa[nk][k][j6];
				
				if(nxsigma[nk][k][j6]<0)
				{
					nxsigma[nk][k][j6]=0;
				}
				if(nysigma[nk][k][j6]<0)
				{
					nysigma[nk][k][j6]=0;
				}
				
				nxsigma[nk][k][j6]=sqrt(nxsigma[nk][k][j6]);
				nysigma[nk][k][j6]=sqrt(nysigma[nk][k][j6]);
			}
		}
	}
}

/* calcuration of measurements */
for(nk=0;nk<Nkotai;nk++)
{
		if(rulenum>(Nrulemax-2))
	{
		break;
	}
	
for(k=0;k<Npn;k++)
{
	if(rulenum>(Nrulemax-2))
	{
		break;
	}

for(i=0;i<10;i++)
{
	asrulepre[i]=0;
	asrulecan[i]=0;
	asruleme[i]=0;
}

for(j1=1;j1<9;j1++)
{
	imx=nxwa[nk][k][j1];
	isx=Maxsigx;
	
	imy=nywa[nk][k][j1];
	isy=Maxsigy;
	
	mnant=kazu[nk][k][j1];
	supp=0;
	sigmadx=nxsigma[nk][k][j1];
	sigmady=nysigma[nk][k][j1];
	
	if(nva[nk][k][j1]!=0)
	{
		supp=(double)mnant/(double)nva[nk][k][j1];
	}

/* conditions for interesting rules */
if(sigmadx<=isx && sigmady<=isy && supp>=Minsup)
{
		for(i2=1;i2<9;i2++)
		{
			asrulepre[i2-1]=zname[nk][k][i2];
		}
		j2=0;
		for(k2=1;k2<(1+Nzk);k2++)
		{
		nflag=0;
			for(i2=0;i2<j1;i2++)
			{
				if(asrulepre[i2]==k2)
				{
					nflag=1;
				}
			}
			if(nflag==1)
			{
				asrulecan[j2]=k2;
				asruleme[j2]=k2;
				j2=j2+1;
			}
		}

if(j2<9)
{
	
/* discrimination of new or not */
	nflag=0;
	for(i2=0;i2<rulenum;i2++)
	{
			sflag1=1;
			
			if(kekka[i2].zen0==asrulecan[0] && kekka[i2].zen1==asrulecan[1] && kekka[i2].zen2==asrulecan[2] && kekka[i2].zen3==asrulecan[3] && kekka[i2].zen4==asrulecan[4] && kekka[i2].zen5==asrulecan[5] && kekka[i2].zen6==asrulecan[6] && kekka[i2].zen7==asrulecan[7])
			{
				sflag1=0;
			}

		if(sflag1==0)
		{
			nflag=1;
			fitness[nk]=fitness[nk]+j2+supp*10+2/(sigmadx+0.1)+2/(sigmady+0.1);

		}
		if(nflag==1)
		{break;}
		
	}/* i2 */
	if(nflag==0)
	{
		kekka[rulenum].zen0=asrulecan[0];
		kekka[rulenum].zen1=asrulecan[1];
		kekka[rulenum].zen2=asrulecan[2];
		kekka[rulenum].zen3=asrulecan[3];
		kekka[rulenum].zen4=asrulecan[4];
		kekka[rulenum].zen5=asrulecan[5];
		kekka[rulenum].zen6=asrulecan[6];
		kekka[rulenum].zen7=asrulecan[7];
		kekka[rulenum].nx_mean=nxwa[nk][k][j1];
		kekka[rulenum].nx_sigma=nxsigma[nk][k][j1];
		kekka[rulenum].ny_mean=nywa[nk][k][j1];
		kekka[rulenum].ny_sigma=nysigma[nk][k][j1];
		kekka[rulenum].n_anter=mnant;
		kekka[rulenum].n_nvalue=nva[nk][k][j1];
		
	fdata5=fopen(frul,"a");
	fprintf(fdata5,"%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",asrulecan[0],asrulecan[1],asrulecan[2],asrulecan[3],asrulecan[4],asrulecan[5],asrulecan[6],asrulecan[7]);
	fclose(fdata5);

		memoh=0;
			
			if(supp>=(Minsup+0.02))
			{
				memoh=1;
			}
		
		kekka[rulenum].mh=memoh;
		
		
		if(kekka[rulenum].mh==1)
		{
			rulenuh=rulenuh+1;
		}
		
		memoj=0;
			
			if(sigmadx<=(Maxsigx-0.1) && sigmady<=(Maxsigy-0.1))
			{
				memoj=1;
			}
		
		kekka[rulenum].mj=memoj;
		
		
		if(kekka[rulenum].mj==1)
		{
			rulenuj=rulenuj+1;
		}

		rulenum=rulenum+1;
		noberule[j2]=noberule[j2]+1;

		fitness[nk]=fitness[nk]+j2+supp*10+2/(sigmadx+0.1)+2/(sigmady+0.1)+20;
		
		for(k3=0;k3<j2;k3++)
			{
				i5=asruleme[k3]-1;
				trkcan[0][i5]=trkcan[0][i5]+1;
			}
			
	}/* if nflag */
	
	if(rulenum>(Nrulemax-2))
	{
		break;
	}
}/* if j2 */
}/* if is<= */
}/* j1 */
	if(rulenum>(Nrulemax-2))
	{
		break;
	}
}/* k */
	if(rulenum>(Nrulemax-2))
	{
		break;
	}
/*
printf("Gen.=%5d \t Ind.=%3d finished.\n",ng,nk);
*/
}/* nk */
	if(rulenum>(Nrulemax-2))
	{
		break;
	}

/* calculation of number of attributes */

trkall=0;
for(i4=0;i4<Nzk;i4++)
{
	att[i4]=0;
	trk[i4]=0;
}
for(i4=0;i4<Nzk;i4++)
{
	for(j4=0;j4<5;j4++)
	{
		trk[i4]=trk[i4]+trkcan[j4][i4];
	}
}

for(i4=0;i4<Nzk;i4++)
{
	for(j4=4;j4>0;j4--)
	{
		trkcan[j4][i4]=trkcan[j4-1][i4];
	}
	
	trkcan[0][i4]=0;
	if(ng%5==0)
	{
		trkcan[0][i4]=1;
	}
}

for(i4=0;i4<Nzk;i4++)
{
	trkall=trkall+trk[i4];
}

for(i4=0;i4<Nkotai;i4++)
{
	for(j4=Npn;j4<(Npn+Njg);j4++)
	{
		att[(geneatt[i4][j4])]=att[(geneatt[i4][j4])]+1;
	}
}

/* ranking */
for(i4=0;i4<Nkotai;i4++)
{
	for(j4=0;j4<Nkotai;j4++)
	{
		if(fitness[j4]>fitness[i4])
		{
			fitrank[i4]=fitrank[i4]+1;
		}
	}
}

/* selection */
for(i4=0;i4<Nkotai;i4++)
{
	if(fitrank[i4]<40)
	{
		for(j4=0;j4<(Npn+Njg);j4++)
		{
		geneatt[fitrank[i4]][j4]=node[i4][j4][0];
		geneatt[fitrank[i4]+40][j4]=node[i4][j4][0];
		geneatt[fitrank[i4]+80][j4]=node[i4][j4][0];
		genecnct[fitrank[i4]][j4]=node[i4][j4][1];
		genecnct[fitrank[i4]+40][j4]=node[i4][j4][1];
		genecnct[fitrank[i4]+80][j4]=node[i4][j4][1];
		}
	}
}

/* crossover */
for(i4=0;i4<20;i4++)
{
	for(j4=0;j4<Nkousa;j4++)
	{
	kousaj1=rand()%Njg+Npn;
		kmemo=genecnct[i4+20][kousaj1];
		genecnct[i4+20][kousaj1]=genecnct[i4][kousaj1];
		genecnct[i4][kousaj1]=kmemo;
		
		kmemo=geneatt[i4+20][kousaj1];
		geneatt[i4+20][kousaj1]=geneatt[i4][kousaj1];
		geneatt[i4][kousaj1]=kmemo;
	}
}

/* mutation of Processing nodes */
for(i4=0;i4<120;i4++)
{
	for(j4=0;j4<Npn;j4++)
	{
	murand=rand()%Muratep;
	if(murand==0)
		{
		genecnct[i4][j4]=rand()%Njg+Npn;
		}
	}
}

/* mutation1 */
for(i4=40;i4<80;i4++)
{
	for(j4=Npn;j4<(Npn+Njg);j4++)
	{
	murand=rand()%Muratej;
	if(murand==0)
		{
		genecnct[i4][j4]=rand()%Njg+Npn;
		}
	}
}

/* mutation2 */
for(i4=80;i4<120;i4++)
{
	/*
	for(j4=Npn;j4<(Npn+Njg);j4++)
	{
	murand=rand()%Muratea;
	if(murand==0)
		{
		geneatt[i4][j4]=rand()%Nzk;
		}
	}
	*/
	
	for(j4=Npn;j4<(Npn+Njg);j4++)
	{
	murand=rand()%Muratea;
	if(murand==0)
		{
			muatt1=rand()%trkall;
			muatt2=0;
			muatt3=0;
			for(i5=0;i5<Nzk;i5++)
			{
				muatt2=muatt2+trk[i5];
				if(muatt1>muatt2)
				{
					muatt3=i5+1;
				}
			}
		geneatt[i4][j4]=muatt3;
		}
	}
}

if(ng%5==0)
{

fdata6=fopen(frap,"a");
	fitaveall=0;

for(i4=0;i4<Nkotai;i4++)
{
	fitaveall=fitaveall+fitness[i4];
}
	fitaveall=fitaveall/Nkotai;

	fprintf(fdata6,"%5d\t%5d\t%5d\t%5d\t%9.3f \n",ng,(rulenum-1),(rulenuh-1),(rulenuj-1),fitaveall);

fclose(fdata6);

printf("Gen.=%5d \t %5d rules extracted (%5d %5d) \n",ng,(rulenum-1),(rulenuh-1),(rulenuj-1));
}

} /* end of main (ng) */

nextrl[fj-Nstart]=rulenum-1;

printf("\n %s rule extraction finished. \n",frul);
printf("==> %5d rules extracted (%5d %5d) \n",nextrl[fj-Nstart],(rulenuh-1),(rulenuj-1));

/* outputs */
fdata9=fopen(floc,"w");
for(i=1;i<rulenum;i++)
{
fprintf(fdata9,"%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t( %2d %2d )\n",kekka[i].zen0,kekka[i].zen1,kekka[i].zen2,kekka[i].zen3,kekka[i].zen4,kekka[i].zen5,kekka[i].zen6,kekka[i].zen7,kekka[i].mh,kekka[i].mj);
fprintf(fdata9,"\n");
}
fclose(fdata9);

/* Rule check (store to big pool) */

if(fj==Nstart)
{
	fdatara=fopen(fpoola,"w");
	for(i2=1;i2<rulenum;i2++)
	{
fprintf(fdatara,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
ditem[kekka[i2].zen0],ditem[kekka[i2].zen1],ditem[kekka[i2].zen2],ditem[kekka[i2].zen3],ditem[kekka[i2].zen4],ditem[kekka[i2].zen5],ditem[kekka[i2].zen6],ditem[kekka[i2].zen7],kekka[i2].nx_mean,kekka[i2].nx_sigma,kekka[i2].ny_mean,kekka[i2].ny_sigma,kekka[i2].n_anter,kekka[i2].n_nvalue,kekka[i2].mh,kekka[i2].mj);
	}
	fclose(fdatara);

	fdatarb=fopen(fpoolb,"w");
	for(i2=1;i2<rulenum;i2++)
	{
fprintf(fdatarb,"%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
kekka[i2].zen0,kekka[i2].zen1,kekka[i2].zen2,kekka[i2].zen3,kekka[i2].zen4,kekka[i2].zen5,kekka[i2].zen6,kekka[i2].zen7,kekka[i2].nx_mean,kekka[i2].nx_sigma,kekka[i2].ny_mean,kekka[i2].ny_sigma,kekka[i2].n_anter,kekka[i2].n_nvalue,kekka[i2].mh,kekka[i2].mj);
	}
	fclose(fdatarb);

	totnum=totnum+rulenum-1;
	totnuh=totnuh+rulenuh-1;
	totnuj=totnuj+rulenuj-1;
}

if(fj>Nstart)
{
for(gj=Nstart;gj<fj;gj++)
	{
	gk5=gj/10000;
	gk4=gj/1000-gk5*10;
	gk3=gj/100-gk5*100-gk4*10;
	gk2=gj/10-gk5*1000-gk4*100-gk3*10;
	gk1=gj-gk5*10000-gk4*1000-gk3*100-gk2*10;

	frul[0]=73;
	frul[1]=76;
	frul[2]=48+gk5;
	frul[3]=48+gk4;
	frul[4]=48+gk3;
	frul[5]=48+gk2;
	frul[6]=48+gk1;
	frul[7]=46;
	frul[8]=116;
	frul[9]=120;
	frul[10]=116;
	frul[11]=0;
/*
printf(" \t %s open \t",frul);
*/
 fdata7=fopen(frul,"r");
 for(i7=1;i7<(nextrl[gj-Nstart]+1);i7++)
{
fscanf(fdata7,"%d%d%d%d%d%d%d%d",&aa[0],&aa[1],&aa[2],&aa[3],&aa[4],&aa[5],&aa[6],&aa[7]);

	cmprule[i7].zen0=aa[0];
	cmprule[i7].zen1=aa[1];
	cmprule[i7].zen2=aa[2];
	cmprule[i7].zen3=aa[3];
	cmprule[i7].zen4=aa[4];
	cmprule[i7].zen5=aa[5];
	cmprule[i7].zen6=aa[6];
	cmprule[i7].zen7=aa[7];
} /*end of for(i7= )*/
fclose(fdata7);

for(i2=1;i2<rulenum;i2++)
{
	for(i7=1;i7<(nextrl[gj-Nstart]+1);i7++)
	{

	if(kekka[i2].zen0==cmprule[i7].zen0 && kekka[i2].zen1==cmprule[i7].zen1 && kekka[i2].zen2==cmprule[i7].zen2 && kekka[i2].zen3==cmprule[i7].zen3 && kekka[i2].zen4==cmprule[i7].zen4 && kekka[i2].zen5==cmprule[i7].zen5 && kekka[i2].zen6==cmprule[i7].zen6 && kekka[i2].zen7==cmprule[i7].zen7)
		{
			newnot[i2]=1;
		}
		
	}/* end of for (i7= ) */
	
}/* end of for (i2= ) */

		printf(" (%4d rules) checked. \n",nextrl[gj-Nstart]);

} /*end of for(gj= )*/

newnotm=0;
newnoth=0;
newnotj=0;


for(i2=1;i2<rulenum;i2++)
{

if(newnot[i2]==0)
{
attm[0]=attset[kekka[i2].zen0];
attm[1]=attset[kekka[i2].zen1];
attm[2]=attset[kekka[i2].zen2];
attm[3]=attset[kekka[i2].zen3];
attm[4]=attset[kekka[i2].zen4];
attm[5]=attset[kekka[i2].zen5];
attm[6]=attset[kekka[i2].zen6];
attm[7]=attset[kekka[i2].zen7];


fdatara=fopen(fpoola,"a");
fprintf(fdatara,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",ditem[attm[0]],ditem[attm[1]],ditem[attm[2]],ditem[attm[3]],ditem[attm[4]],ditem[attm[5]],ditem[attm[6]],ditem[attm[7]],kekka[i2].nx_mean,kekka[i2].nx_sigma,kekka[i2].ny_mean,kekka[i2].ny_sigma,kekka[i2].n_anter,kekka[i2].n_nvalue,kekka[i2].mh,kekka[i2].mj);
fclose(fdatara);

fdatarb=fopen(fpoolb,"a");
fprintf(fdatarb,"%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",attm[0],attm[1],attm[2],attm[3],attm[4],attm[5],attm[6],attm[7],kekka[i2].nx_mean,kekka[i2].nx_sigma,kekka[i2].ny_mean,kekka[i2].ny_sigma,kekka[i2].n_anter,kekka[i2].n_nvalue,kekka[i2].mh,kekka[i2].mj);
fclose(fdatarb);

newnotm=newnotm+1;

	if(kekka[i2].mh==1)
	{
		newnoth=newnoth+1;
	}
	if(kekka[i2].mj==1)
	{
		newnotj=newnotj+1;
	}
}

} /* for(i2= )*/

	printf("\t %4d rules are new. (%4d %4d) \n",newnotm,newnoth,newnotj);
	totnum=totnum+newnotm;
	totnuh=totnuh+newnoth;
	totnuj=totnuj+newnotj;

}/*end of if(fj!=Nstart)*/

printf("\t total= %7d rules (%4d %4d)\n\n",totnum,totnuh,totnuj);

fdatac=fopen(fcont,"a");
fprintf(fdatac,"%d\t%d\t%d\t%d\t%d\t%d\t%d\n",(fj-Nstart+1),(rulenum-1),(rulenuh-1),(rulenuj-1),totnum,totnuh,totnuj);
fclose(fdatac);

}/*end of for(fj= )*/

return;
}
