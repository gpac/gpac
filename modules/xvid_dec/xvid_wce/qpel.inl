static void FUNC_H(byte *Dst, const byte *Src, int H, int BpS, int Rnd){
   while(H-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      for(i=0; i<=SIZE; ++i)
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * Src[i];

      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd ) >> 5;
         if (C<0) C = 0; else if (C>255) C = 255;
         STORE(Dst[i], C);
      }
      Src += BpS;
      Dst += BpS;
   }
}

static void FUNC_V(byte *Dst, const byte *Src, int W, int BpS, int Rnd){
   while(W-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      const byte *S = Src++;
      byte *D = Dst++;
      for(i=0; i<=SIZE; ++i) {
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * S[0];
         S += BpS;
      }

      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd )>>5;
         if (C<0) C = 0; else if (C>255) C = 255;
         STORE(D[0], C);
         D += BpS;
      }
   }
}

static
void FUNC_HA(byte *Dst, const byte *Src, int H, int BpS, int Rnd)
{
   while(H-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      for(i=0; i<=SIZE; ++i)
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * Src[i];

      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd ) >> 5;
         if (C<0) C = 0; else if (C>255) C = 255;
         C = (C+Src[i]+1-Rnd) >> 1;
         STORE(Dst[i], C);
      }
      Src += BpS;
      Dst += BpS;
   }
}

static
void FUNC_HA_UP(byte *Dst, const byte *Src, int H, int BpS, int Rnd)
{
   while(H-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      for(i=0; i<=SIZE; ++i)
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * Src[i];

      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd ) >> 5;
         if (C<0) C = 0; else if (C>255) C = 255;
         C = (C+Src[i+1]+1-Rnd) >> 1;
         STORE(Dst[i], C);
      }
      Src += BpS;
      Dst += BpS;
   }
}

static
void FUNC_VA(byte *Dst, const byte *Src, int W, int BpS, int Rnd)
{
   while(W-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      const byte *S = Src;
      byte *D = Dst;

      for(i=0; i<=SIZE; ++i) {
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * S[0];
         S += BpS;
      }

      S = Src;
      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd )>>5;
         if (C<0) C = 0; else if (C>255) C = 255;
         C = ( C+S[0]+1-Rnd ) >> 1;
         STORE(D[0], C);
         D += BpS;
         S += BpS;
      }
      Src++;
      Dst++;
   }
}

static
void FUNC_VA_UP(byte *Dst, const byte *Src, int W, int BpS, int Rnd)
{
   while(W-->0) {
      int i, k;
      int Sums[SIZE] = { 0 };
      const byte *S = Src;
      byte *D = Dst;

      for(i=0; i<=SIZE; ++i) {
         for(k=0; k<SIZE; ++k)
            Sums[k] += TABLE[i][k] * S[0];
         S += BpS;
      }

      S = Src + BpS;
      for(i=0; i<SIZE; ++i) {
         int C = ( Sums[i] + 16-Rnd )>>5;
         if (C<0) C = 0; else if (C>255) C = 255;
         C = ( C+S[0]+1-Rnd ) >> 1;
         STORE(D[0], C);
         D += BpS;
         S += BpS;
      }
      Dst++;
      Src++;
   }
}

#undef STORE
#undef FUNC_H
#undef FUNC_V
#undef FUNC_HA
#undef FUNC_VA
#undef FUNC_HA_UP
#undef FUNC_VA_UP
