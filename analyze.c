/**
 * @file analyze.c
 * 
 * @brief Modul na anal�zu fragment�cie disku
 *
 */
/* Modul som za�al p�sa� d�a: 2.11.2006 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <fat32.h>
#include <analyze.h>

/** Tabulka s d�le�it�mi inform�ciami o ka�dej polo�ke v celej adres�rovej �trukt�re.
 * Jednotliv� polo�ky tejto tabulky obsahuj�: �tartovac� klaster, ��slo adres�rov�ho klastra, ��slo
 * polo�ky v adres�rovom klastri, po�et klastrov (t�to hodnota sa vypln� a� pri h�bkovej anal�ze).
 * Defragment�cia pracuje na z�klade �dajov v tejto tabulke.
 */
aTableItem* aTable = NULL;

/** Po�et hodn�t v tabulke je rovn� po�tu v�etk�ch s�borov
  * a adres�rov, ktor� maj� alokovan� aspo� 1 klaster
  */
unsigned long tableCount = 0;

/** Percentu�lna diskov� fragment�cia */
float diskFragmentation;

/** Po�et pou�it�ch klastrov */
unsigned long usedClusters;

/** Po�et polo�iek v jednom adres�ri (premenliv� hodnota podla velkosti klastra) */
unsigned short entryCount;

/** Naplnenie tabulky aTable sa deje rekurz�vne, tabulka je implementovan�
  * ako dynamick� pole o max. 10000 polo�iek (�i�e m��e existova� max 10000
  * s�borov a adres�rov spolu na disku). Vhodnej�ia by bola mo�no implement�cia
  * pomocou spojkov�ho zoznamu, no pre ��ely zadania mysl�m posta�� aj dynamick� pole.
  * T�to funkcia prid� nov� polo�ku do aTable, vypln� v�ak iba niektor� inform�cie v polo�ke
  * @param startCluster �tartovac� klaster polo�ky
  * @param entCluster klaster adres�ra, ktor� odkazuje na polo�ku
  * @param ind index, poradov� ��slo polo�ky v adres�rovom klastri
  */
void an_addFile(unsigned long startCluster, unsigned long entCluster, unsigned short ind)
{
  if (aTable == NULL) {
    if ((aTable = (aTableItem *)malloc(10000 * sizeof(aTableItem))) == NULL)
      error(0, gettext("Out of memory !"));
    tableCount = 0;
  } else;
  tableCount++;
  if (tableCount >= 10000) { free(aTable); error(0,gettext("Out of memory !"));}
  aTable[tableCount-1].startCluster = startCluster;
  aTable[tableCount-1].entryCluster = entCluster;
  aTable[tableCount-1].entryIndex = ind;
}

/** Funkcia uvoln� pam� pou�it� pre aTable
  */
void an_freeTable()
{
  free(aTable);
}

/** Funkcia zist� percentu�lnu fragment�ciu jednej adres�rovej
  * polo�ky (s�boru / adres�ra). Je s��as�ou h�bkovej anal�zy disku.
  * Pracuje tak, �e pre dan� �tartovac� klaster prejde
  * celou re�azou a zis�uje, �i klastre id� za sebou, �i�e �i rozdiel nasleduj�ceho a 
  * aktu�lneho klastra d�va 1. V pr�pade, �e nie, inkrementuje sa po�et fragmentovan�ch klastrov.
  * Po�as prechodu celej re�aze sa uchov�va celkov� po�et klastrov, ktor�mi sa pre�lo. Po opusten�
  * cyklu prechodu re�azou ud�va t�to premenn� po�et klastrov s�boru/adres�ra a je ulo�en� do aTable.
  * Percentu�lna fragment�cia sa vypo��ta podla vz�ahu:
  * \code
  *   (pocet frag.klastrov polozky) / (pocet vsetkych pouzitych klastrov polozky) * 100
  * \endcode
  * @param startCluster �tartovac� klaster polo�ky
  * @param aTIndex index v tabulke aTable - do tabulky sa zap�e po�et klastrov adres�rovej polo�ky
  * @return vracia fragment�ciu polo�ky v percent�ch
*/
float an_getFileFragmentation(unsigned long startCluster, unsigned long aTIndex)
{
  unsigned long cluster; /* pomocny cluster */
  int fragmentCount = 0; /* pocet fragmentovanych clusterov */
  int count = 0;	 /* pocet clusterov suboru; neratam start.cluster */
  
  for (cluster = startCluster, count=0; !F32_LAST(cluster); cluster = f32_getNextCluster(cluster),count++) {
    if ((startCluster != cluster) && (startCluster+1 != cluster))
      fragmentCount++;
    startCluster = cluster;
  }
  if (F32_LAST(cluster)) count++;
  usedClusters += count;
  aTable[aTIndex].clusterCount = count;
  return (float)(((float)fragmentCount / (float)count) * 100.0);
}

/** Funkcia rekurz�vne traverzuje celou adres�rovou �trukt�rou. Je s��as�ou prvej f�zy anal�zy disku (z�kladn�).
  * Po�as prechodu uklad� do aTable d�le�it� inform�cie o adres�rovej polo�ke, ako je �tartovac� klaster,
  * ��slo klastra adres�ra, ktor� obsahuje odkaz na dan� polo�ku a index v adres�rovom klastri. Okrem toho vol�
  * pre ka�d� polo�ku zistenie jej percentu�lnej fragment�cie. T�to sa pripo��ta ku glob�lnej premennej diskFragmentation.
  * Pozor! FATka mus� by� v poriadku, lebo inak m��e vznikn�� zacyklenie (pri kr�ov�ch referenci�ch) !
  * @param startCluster ��slo root klastra (odkial sa bude rekurz�vne traverzova�)
*/
void an_scanDisk(unsigned long startCluster)
{
  unsigned short index;
  unsigned long cluster, tmpCluster;
  unsigned char tmpAttr;
  F32_DirEntry *entries;
  
  /* v chybnej FATke musim ratat s clusterCount miesto 0xffffff0 */
  if (startCluster > info.clusterCount) return;

  if ((entries = (F32_DirEntry *)malloc(entryCount * sizeof(F32_DirEntry))) == NULL)
    error(0, gettext("Out of memory !"));

  for (cluster = startCluster; !F32_LAST(cluster); cluster = f32_getNextCluster(cluster)) {
    f32_readCluster(cluster, entries);
    for (index = 0; index < entryCount; index++) {
      if (!entries[index].fileName[0]) { free(entries); return; }
      /* dalej pracujem iba s polozkami, ktore:
           1. nie su zmazane,
	   2. nie su to sloty (dlhe nazvy)
	   3. neodkazuju na rodica alebo na korenovy adresar
      */
      if ((entries[index].fileName[0] != 0xe5) && 
          entries[index].attributes != 0x0f &&
          memcmp(entries[index].fileName,".       ",8) &&
	  memcmp(entries[index].fileName,"..      ",8)) {
	tmpAttr = entries[index].attributes & 0x10;
        tmpCluster = f32_getStartCluster(entries[index]);
	if (tmpCluster != 0) {
	  /* ak je polozka podadresar, rekurzivne sa vola funkcia */
          if (tmpAttr == 0x10) {
	    /* ochrana proti zacykleniu */
	    if (tmpCluster != startCluster)
	      an_scanDisk(tmpCluster);
	  }
          /* ak je startovaci cluster chybny, ignoruje polozku */
          if (tmpCluster <= info.clusterCount) {
            an_addFile(tmpCluster,cluster,index);
            diskFragmentation += an_getFileFragmentation(tmpCluster, tableCount-1);
          }
	}
      }
    }
  }
  free(entries);
}

/** Hlavn� funkcia pre anal�zu disku; predt�m, ako zavol� funkciu an_scanDisk vykon�
  * nejak� pr�pravn� oper�cie, ako zist� po�et polo�iek v direntry a prid� prv� hodnotu
  * do aTable - root klaster, pre ktor� tie� vypo��ta jeho fragment�ciu. Po ukon�en�
  * rekurz�vneho traverzovania adres�rovej �trukt�ry vypo��ta celkov� percentu�lnu
  * fragment�ciu disku vydelen�m premennej diskFragmentation po�tom polo�iek v aTable.
  */
int an_analyze()
{
  fprintf(output_stream, gettext("Analysing disk...\n"));

  entryCount = (bpb.BPB_SecPerClus * info.BPSector) / sizeof(F32_DirEntry);
  /* prva faza analyzy zacina root clusterom */
  aTable = NULL;
  tableCount = 0;
  
  /* tabulka obsahuje aj root cluster */
  an_addFile(bpb.BPB_RootClus, 0, 0);
  usedClusters = 0;
  diskFragmentation = an_getFileFragmentation(bpb.BPB_RootClus, 0);
  an_scanDisk(bpb.BPB_RootClus);
  diskFragmentation /= (tableCount - 1);

  fprintf(output_stream, gettext("Disk is fragmented for: %.2f%%\n"), diskFragmentation);

  /*POZOR! Neuvolnujem pamat tabulky teraz, ale AZ PO defragmentacii,
    inak by vznikla chyba "Segmentation fault" vzhladom na to, ze sa
    tabulka bude pouzivat neskor.
  */
  return 0;
}
