#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAKS_PROCESA 10
#define MAKS_OKVIRA 50
#define VELICINA_OKVIRA 64
#define BROJ_STRANICA 16

int brojProcesa, brojOkvira, brojacVremena;
char swapProstor[MAKS_PROCESA][BROJ_STRANICA][VELICINA_OKVIRA];
short tablicaStranica[MAKS_PROCESA][BROJ_STRANICA];
char okviri[MAKS_OKVIRA][VELICINA_OKVIRA];
long mapaOkvira[MAKS_OKVIRA];
int okvirZaIzbaciti;

void inicijaliziraj() {
    srand(time(NULL));
    memset(swapProstor, 0, sizeof(swapProstor));
    memset(okviri, 0, sizeof(okviri));
    memset(tablicaStranica, 0, sizeof(tablicaStranica));
    for (int i = 0; i < MAKS_OKVIRA; i++) {
        mapaOkvira[i] = -1; 
    }
    brojacVremena = 0;
    okvirZaIzbaciti = 0;
}

void resetirajVremenskeZigove() {
    for (int i = 0; i < brojProcesa; i++) {
        for (int j = 0; j < BROJ_STRANICA; j++) {
            tablicaStranica[i][j] &= 0xffc0; 
        }
    }
}

void odaberiOkvirZaIzbaciti() {
    int minLRU = -1;
    for (int i = 0; i < brojOkvira; i++) {
        if (mapaOkvira[i] == -1) { 
            okvirZaIzbaciti = i;
            return;
        }
        int proces = (int)((mapaOkvira[i] >> 32) & 0xffffffff);
        int stranica = (int)(mapaOkvira[i] & 0xffff);
        int lruVrijednost = tablicaStranica[proces][stranica] & 0b11111;
        if (minLRU == -1 || lruVrijednost < minLRU) { 
            minLRU = lruVrijednost;
            okvirZaIzbaciti = i;
        }
    }
}

char* rjesiAdresu(int proces, int logickaAdresa) {
    if (proces < 0 || proces >= brojProcesa) {
        fprintf(stderr, "Greška: Neispravan proces ID %d\n", proces);
        return NULL;
    }

    if (logickaAdresa < 0 || logickaAdresa >= (BROJ_STRANICA * VELICINA_OKVIRA)) {
        fprintf(stderr, "Greška: Neispravna logička adresa %d\n", logickaAdresa);
        return NULL;
    }

    int indeksStranice = logickaAdresa >> 6;
    short zapis = tablicaStranica[proces][indeksStranice];
    int prisutna = (zapis & 0b100000) >> 5;
    int indeksOkvira = zapis >> 6;

    if (!prisutna) {
        printf("Promasaj!\n");
        odaberiOkvirZaIzbaciti();
        if (mapaOkvira[okvirZaIzbaciti] != -1) {
            int stariProces = (int)((mapaOkvira[okvirZaIzbaciti] >> 32) & 0xffffffff);
            int staraStranica = (int)(mapaOkvira[okvirZaIzbaciti] & 0xffff);
            printf("Izbacujem stranicu 0x%04x iz procesa %d\n", staraStranica * VELICINA_OKVIRA, stariProces);
            memcpy(swapProstor[stariProces][staraStranica], okviri[okvirZaIzbaciti], VELICINA_OKVIRA);
            tablicaStranica[stariProces][staraStranica] = 0;
        }
        printf("dodijeljen okvir 0x%04x\n", okvirZaIzbaciti);
        mapaOkvira[okvirZaIzbaciti] = ((long)proces << 32) | indeksStranice;
        indeksOkvira = okvirZaIzbaciti;
        memcpy(okviri[indeksOkvira], swapProstor[proces][indeksStranice], VELICINA_OKVIRA);
    }

    if (brojacVremena == 31) {
        resetirajVremenskeZigove();
        brojacVremena = 1;
    }
    tablicaStranica[proces][indeksStranice] = (indeksOkvira << 6) | 0b100000 | brojacVremena;

    int pomak = logickaAdresa & 0b111111;
    int fizickaAdresa = (indeksOkvira << 6) | pomak;
    printf("fiz. adresa: 0x%04x\n", fizickaAdresa);
    printf("zapis tablice: 0x%04x\n", tablicaStranica[proces][indeksStranice]);
    printf("sadrzaj adrese: %d\n", okviri[indeksOkvira][pomak]);

    return okviri[indeksOkvira] + pomak;
}

char citajBajt(int proces, int logickaAdresa) {
    char *fizickaAdresa = rjesiAdresu(proces, logickaAdresa);
    if (fizickaAdresa == NULL) {
        fprintf(stderr, "Greška: Ne mogu pročitati bajt na adresi %d\n", logickaAdresa);
        return -1; 
    }
    return *fizickaAdresa;
}

void pisiBajt(int proces, int logickaAdresa, char vrijednost) {
    char *fizickaAdresa = rjesiAdresu(proces, logickaAdresa);
    if (fizickaAdresa == NULL) {
        fprintf(stderr, "Greška: Ne mogu pisati bajt na adresi %d\n", logickaAdresa);
        return;
    }
    *fizickaAdresa = vrijednost;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <brojProcesa> <brojOkvira>\n", argv[0]);
        return 1;
    }

    brojProcesa = atoi(argv[1]);
    brojOkvira = atoi(argv[2]);

    if (brojProcesa <= 0 || brojProcesa > MAKS_PROCESA) {
        fprintf(stderr, "Greška: Neispravan broj procesa. Maksimalno dozvoljeno je %d\n", MAKS_PROCESA);
        return 1;
    }

    if (brojOkvira <= 0 || brojOkvira > MAKS_OKVIRA) {
        fprintf(stderr, "Greška: Neispravan broj okvira. Maksimalno dozvoljeno je %d\n", MAKS_OKVIRA);
        return 1;
    }

    inicijaliziraj();

    while (1) {
        for (int p = 0; p < brojProcesa; p++) {
            printf("-----------------------------\n");
            printf("proces: %d\n", p);

            int logickaAdresa = rand() % (BROJ_STRANICA * VELICINA_OKVIRA);
            printf("t: %d\n", brojacVremena);
            printf("log. adresa: 0x%04x\n", logickaAdresa);

            char podatak = citajBajt(p, logickaAdresa);
            if (podatak == -1) continue; 
            printf("Podatak na adresi: %d\n", podatak);

            podatak++;
            pisiBajt(p, logickaAdresa, podatak);
            brojacVremena++;

            sleep(1);
        }
    }

    return 0;
}
