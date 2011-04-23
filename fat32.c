/**
 * @file fat32.c
 *
 * @brief Modul implementuje oper�cie so s�borov�m syst�mom FAT32
 *
 * Modul implementuje z�kladn� funkcie pre pr�cu so s�borov�m syst�mom FAT - s� tu funkcie ako zistenie typu FAT,
 * pre��tanie a z�pis hodn�t do FAT, pre��tanie a z�pis klastrov a podobne. Modul priamo vyu��va funkcie modulu disk.c,
 * d� sa poveda�, �e je ak�msi vy���m levelom.
 *
 * E�te jedna zauj�mavos�: Pre ��tanie a z�pis hodn�t do FAT tabulky je pou�it� cache, ktor� obsahuje cel� sektor, v
 * ktorom sa nach�dza hodnota, s ktorou sa pracuje. T�to cache sa aktualizuje, ak je pracovn� hodnota mimo jej rozsahu.
 * Pom�ha tak k ur�chleniu pr�ce samotn�ho procesu defragment�cie.
 *
 */
/* Modul som za�al p�sa� d�a: 1.11.2006 */

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <disk.h>
#include <fat32.h>

/** glob�lna premenn� BIOS Parameter Block */
F32_BPB bpb;
/** inform�cie o syst�me (v�ber hodn�t z bpb plus �al�ie hodnoty, ako za�iatok d�tovej oblasti, apod. ) */
F32_Info info;

unsigned long *cacheFsec; /** cache pre jeden sektor FAT tabulky */
static unsigned short cacheFindex = 0; /** ��slo cachovan�ho sektora (log.LBA) */

/** Funkcia zist� typ FAT a napln� �trukt�ru info; bpb mus� u� by� na��tan�.
  * Typ FAT sa d� korektne zisti� (podla Microsoftu) jedine podla
  * po�tu klastrov vo FAT. Ak je ich po�et < 4085, ide o FAT12, ak je < 65525 ide o FAT16, inak o FAT32.
  * Tato determin�cia typu FAT je v�ak ur�en� pre skuto�n� FAT-ky; ke�e obraz m� niekedy len 1MB a tam
  * sa vojde asi okolo 1000 klastrov, "korektn�" determin�cia zlyh�va a v�sledok by bol FAT12. Preto som musel
  * determin�ciu urobi� "nekorektnou" a pou�i� zistenie podla bpb.BS_FilSysType (obsahuje bu� "FAT12   ", "FAT16   ",
  * alebo "FAT32   "), kde zistenie typu FAT podla tohto parametra pova�uje Microsoft za nepr�pustn�...
  * @return Funkcia vracia typ s�borov�ho syst�mu (kon�tanty definovan� vo fat32.h)
  */
int f32_determineFATType()
{
  unsigned long rootDirSectors, totalSectors, DataSector;

  rootDirSectors = ((bpb.BPB_RootEntCnt * 32) + (bpb.BPB_BytesPerSec-1)) / (bpb.BPB_BytesPerSec);
  totalSectors = (bpb.BPB_TotSec16) ? (unsigned long)bpb.BPB_TotSec16 : bpb.BPB_TotSec32;

  info.FATstart = (unsigned long)bpb.BPB_RsvdSecCnt;
  info.BPSector = bpb.BPB_BytesPerSec;
  info.fSecClusters = info.BPSector / 4;
  info.FATsize = (bpb.BPB_FATSz16) ? (unsigned long)bpb.BPB_FATSz16 : bpb.BPB_FATSz32;
  info.firstDataSector = bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * info.FATsize;
  info.firstRootSector = info.firstDataSector + (bpb.BPB_RootClus - 2) * bpb.BPB_SecPerClus;
  info.clusterCount = (totalSectors - (info.FATstart + (info.FATsize * bpb.BPB_NumFATs) + rootDirSectors )) / bpb.BPB_SecPerClus + 1;
                         
  if (info.clusterCount < 4085 && !strcmp(bpb.BS_FilSysType,"FAT12   ")) return FAT12;
  else if (info.clusterCount < 65525L && !strcmp(bpb.BS_FilSysType,"FAT16   ")) return FAT16;
  else if (!memcmp(bpb.BS_FilSysType,"FAT32   ",8)) return FAT32;
  else
    error(0,gettext("Can't determine FAT type (label: '%s')\n"), bpb.BS_FilSysType);
}


/** Namontovanie s�borov�ho syst�mu FAT32, znamen� to vlastne:
 *  -# zisti�, �i je typ FS naozaj FAT32
 *  -# zisti� dodato�n� inform�cie o FATke (naplni� �trukt�ru F32_Info)
 *  -# alokova� pam� pre cache
 *
 * @return ak nebola �iadna chyba, vracia 0.
 */
int f32_mount(int image_descriptor)
{
  int ftype;
  d_mount(image_descriptor); /* namontujem disk, aby bolo mozne pouzivat diskove operacie */

  /* Nacitanie BPB */
  if (d_readSectors(0, (char*)&bpb, 1, 512) != 1)
    error(0,gettext("Can't read BPB !"));

  if (debug_mode) {
    fprintf(output_stream, gettext("(f32_mount) BIOS Parameter Block (BPB):\n"));
    fprintf(output_stream, gettext("(f32_mount) BPB_jmpBoot: '%s', offset: %x, size: %d\n"), bpb.BS_jmpBoot, (int)((int)(&bpb.BS_jmpBoot) - (int)(&bpb)), sizeof(bpb.BS_jmpBoot));
    fprintf(output_stream, gettext("(f32_mount) BS_OEMName: '%s', offset: %x, size: %d\n"), bpb.BS_OEMName, (int)((int)(&bpb.BS_OEMName) - (int)(&bpb)), sizeof(bpb.BS_OEMName));
    fprintf(output_stream, gettext("(f32_mount) BPB_BytesPerSec: %d, offset: %x, size: %d\n"), (short)bpb.BPB_BytesPerSec, (int)((int)(&bpb.BPB_BytesPerSec) - (int)(&bpb)), sizeof(bpb.BPB_BytesPerSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_SecPerClus: %u, offset: %x, size: %d\n"), bpb.BPB_SecPerClus, (int)((int)(&bpb.BPB_SecPerClus) - (int)(&bpb)), sizeof(bpb.BPB_SecPerClus));
    fprintf(output_stream, gettext("(f32_mount) BPB_RsvdSecCnt: %d, offset: %x, size: %d\n"), bpb.BPB_RsvdSecCnt, (int)((int)(&bpb.BPB_RsvdSecCnt) - (int)(&bpb)), sizeof(bpb.BPB_RsvdSecCnt));
    fprintf(output_stream, gettext("(f32_mount) BPB_NumFATs: %d, offset: %x, size: %d\n"), bpb.BPB_NumFATs, (int)((int)(&bpb.BPB_NumFATs) - (int)(&bpb)), sizeof(bpb.BPB_NumFATs));
    fprintf(output_stream, gettext("(f32_mount) BPB_RootEntCnt: %d, offset: %x, size: %d\n"), bpb.BPB_RootEntCnt, (int)((int)(&bpb.BPB_RootEntCnt) - (int)(&bpb)), sizeof(bpb.BPB_RootEntCnt));
    fprintf(output_stream, gettext("(f32_mount) BPB_TotSec16: %d, offset: %x, size: %d\n"), bpb.BPB_TotSec16, (int)((int)(&bpb.BPB_TotSec16) - (int)(&bpb)), sizeof(bpb.BPB_TotSec16));
    fprintf(output_stream, gettext("(f32_mount) BPB_Media: %x, offset: %x, size: %d\n"), bpb.BPB_Media, (int)((int)(&bpb.BPB_Media) - (int)(&bpb)), sizeof(bpb.BPB_Media));
    fprintf(output_stream, gettext("(f32_mount) BPB_FATSz16: %d, offset: %x, size: %d\n"), bpb.BPB_FATSz16, (int)((int)(&bpb.BPB_FATSz16) - (int)(&bpb)), sizeof(bpb.BPB_FATSz16));
    fprintf(output_stream, gettext("(f32_mount) BPB_SePerTrk: %d, offset: %x, size: %d\n"), bpb.BPB_SecPerTrk, (int)((int)(&bpb.BPB_SecPerTrk) - (int)(&bpb)), sizeof(bpb.BPB_SecPerTrk));
    fprintf(output_stream, gettext("(f32_mount) BPB_NumHeads: %d, offset: %x, size: %d\n"), bpb.BPB_NumHeads, (int)((int)(&bpb.BPB_NumHeads) - (int)(&bpb)), sizeof(bpb.BPB_NumHeads));
    fprintf(output_stream, gettext("(f32_mount) BPB_HiddSec: %d, offset: %x, size: %d\n"), bpb.BPB_HiddSec, (int)((int)(&bpb.BPB_HiddSec) - (int)(&bpb)), sizeof(bpb.BPB_HiddSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_TotSec32: %ld, offset: %x, size: %d\n"), bpb.BPB_TotSec32, (int)((int)(&bpb.BPB_TotSec32) - (int)(&bpb)), sizeof(bpb.BPB_TotSec32));
    fprintf(output_stream, gettext("(f32_mount) BPB_FATSz32: %ld, offset: %x, size: %d\n"), bpb.BPB_FATSz32, (int)((int)(&bpb.BPB_FATSz32) - (int)(&bpb)), sizeof(bpb.BPB_FATSz32));
    fprintf(output_stream, gettext("(f32_mount) BPB_ExtFlags: %d, offset: %x, size: %d\n"), bpb.BPB_ExtFlags, (int)((int)(&bpb.BPB_ExtFlags) - (int)(&bpb)), sizeof(bpb.BPB_ExtFlags));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSVer major: %d, offset: %x, size: %d\n"), bpb.BPB_FSVerMajor, (int)((int)(&bpb.BPB_FSVerMajor) - (int)(&bpb)), sizeof(bpb.BPB_FSVerMajor));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSVer minor: %d, offset: %x, size: %d\n"), bpb.BPB_FSVerMinor, (int)((int)(&bpb.BPB_FSVerMinor) - (int)(&bpb)), sizeof(bpb.BPB_FSVerMinor));
    fprintf(output_stream, gettext("(f32_mount) BPB_RootClus: %ld, offset: %x, size: %d\n"), bpb.BPB_RootClus, (int)((int)(&bpb.BPB_RootClus) - (int)(&bpb)), sizeof(bpb.BPB_RootClus));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSInfo: %d, offset: %x, size: %d\n"), bpb.BPB_FSInfo, (int)((int)(&bpb.BPB_FSInfo) - (int)(&bpb)), sizeof(bpb.BPB_FSInfo));
    fprintf(output_stream, gettext("(f32_mount) BPB_BkBootSec: %d, offset: %x, size: %d\n"), bpb.BPB_BkBootSec, (int)((int)(&bpb.BPB_BkBootSec) - (int)(&bpb)), sizeof(bpb.BPB_BkBootSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_Reserved: '%s', offset: %x, size: %d\n"), bpb.BPB_Reserved2, (int)((int)(&bpb.BPB_Reserved2) - (int)(&bpb)), sizeof(bpb.BPB_Reserved2));
    fprintf(output_stream, gettext("(f32_mount) BS_DrvNum: %d, offset: %x, size: %d\n"), bpb.BS_DrvNum, (int)((int)(&bpb.BS_DrvNum) - (int)(&bpb)), sizeof(bpb.BS_DrvNum));
    fprintf(output_stream, gettext("(f32_mount) BS_Reserved1: %d, offset: %x, size: %d\n"), bpb.BS_Reserved1, (int)((int)(&bpb.BS_Reserved1) - (int)(&bpb)), sizeof(bpb.BS_Reserved1));
    fprintf(output_stream, gettext("(f32_mount) BS_BootSig: %d, offset: %x, size: %d\n"), bpb.BS_BootSig, (int)((int)(&bpb.BS_BootSig) - (int)(&bpb)), sizeof(bpb.BS_BootSig));
    fprintf(output_stream, gettext("(f32_mount) BS_VolID: %ld, offset: %x, size: %d\n"), bpb.BS_VolID, (int)((int)(&bpb.BS_VolID) - (int)(&bpb)), sizeof(bpb.BS_VolID));
    fprintf(output_stream, gettext("(f32_mount) BS_VolLab: '%s', offset: %x, size: %d\n"), bpb.BS_VolLab, (int)((int)(&bpb.BS_VolLab) - (int)(&bpb)), sizeof(bpb.BS_VolLab));
    fprintf(output_stream, gettext("(f32_mount) BS_FilSysType: '%s', offset: %x, size: %d\n"), bpb.BS_FilSysType, (int)((int)(&bpb.BS_FilSysType) - (int)(&bpb)), sizeof(bpb.BS_FilSysType));
  }
  /* kontrola, ci je to FAT32 (podla Microsoftu nespravna) */
  if ((ftype = f32_determineFATType()) != FAT32)
    error(0,gettext("File system on image isn't FAT32, but FAT%d !"),ftype);
  
  /* zisti, ci je FAT zrkadlena */
  if (!(bpb.BPB_ExtFlags & 0x80)) info.FATmirroring = 1;
  else {
    info.FATmirroring = 0;
    info.FATstart += (bpb.BPB_ExtFlags & 0x0F) * info.FATsize; /* ak nie, nastavi sa na aktivnu FAT */
  }
  if ((cacheFsec = (unsigned long *)malloc(sizeof(unsigned long) * info.fSecClusters)) == NULL)
    error(0,gettext("Out of memory !"));

  return 0;
}

/** Funkcia zist�, �i je FAT32 namontovan�; je to vtedy, ke� nie je nulov� FATstart a ke� je namontovan� disk
 *  @return 1 ak je namontovan� FAT32, inak 0
 */
int f32_mounted()
{
  if (info.FATstart && d_mounted()) return 1;
  else return 0;
}

/** Funkcia odmontuje FAT, �i�e vynuluje FATstart, uvoln� pam� cache a odmontuje disk */
int f32_umount()
{
  info.FATstart = 0;
  free(cacheFsec);
  d_umount();
  return 0;
}

/** Funkcia pre��ta hodnotu klastra vo FAT tabulke (vr�ti ju vo value), vyu��va sa cache FATky.
 *  Je tu implementovan� iba F32 verzia, �i�e funkcia nie je pou�iteln� pre FAT12/16.
 *  @param cluster ��slo klastra, ktor�ho hodnota sa z FAT pre��ta
 *  @param value[v�stup] do tohto smern�ka sa ulo�� pre��tan� hodnota
 *  @return v pr�pade, ak nenastala chyba, fukncia vr�ti 0.
 */
int f32_readFAT(unsigned long cluster, unsigned long *value)
{
  unsigned long logicalLBA;
  unsigned short index;
  unsigned long val;
  
  if (!f32_mounted()) return 1;
  
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector); /* sektor FAT, ktory obsahuje cluster */
  index = (cluster % info.fSecClusters); /* index v sektore FAT tabulky */
  if (logicalLBA > (info.FATstart + info.FATsize))
    error(0,gettext("Trying to read cluster > max !"));

  if (cacheFindex != logicalLBA) {
    if (d_readSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1) error(0,gettext("Can't read from image (pos.:0x%lx)!"), logicalLBA);
    else cacheFindex = logicalLBA;
  }

  val = cacheFsec[index] & 0x0fffffff;
  *value = val;
  return 0;
}

/** Funkcia zap�e hodnotu klastra do FAT tabulky, vyu��va sa cache FATky.
 *  Je tu implementovan� iba F32 verzia, �i�e funkcia nie je pou�iteln� pre FAT12/16. V pr�pade, �e je zapnut�
 *  zrkadlenie FATky, je hodnota zap�san� aj do druhej k�pie (predpokladaj� sa iba dve k�pie).
 *  @param cluster ��slo klastra, do ktor�ho sa zap�e hodnota vo FAT
 *  @param value ur�uje hodnotu, ktor� bude zap�san� do FAT
 *  @return v pr�pade, ak nenastala chyba, fukncia vr�ti 0.
 */
int f32_writeFAT(unsigned long cluster, unsigned long value)
{
  unsigned long logicalLBA;
  unsigned short index;

  if (!f32_mounted()) return 1;
    
  value &= 0x0fffffff;
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector);
  index = (cluster % info.fSecClusters); /* index v sektore FAT tabulky */
  if (logicalLBA > (info.FATstart + info.FATsize))
    error(0,gettext("Trying to write cluster > max !"));
  
  if (cacheFindex != logicalLBA) {
    if (d_readSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1) error(0,gettext("Can't read from image (pos.:0x%lx) !"), logicalLBA);
    else cacheFindex = logicalLBA;
  }
  cacheFsec[index] = cacheFsec[index] & 0xf0000000;
  cacheFsec[index] = cacheFsec[index] | value;

  if (d_writeSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1)
    error(0,gettext("Can't write to image (pos.:0x%lx) !"), logicalLBA);

  if (info.FATmirroring)
    /* predpokladam, ze su iba 2 kopie FAT */
    if (d_writeSectors(logicalLBA + info.FATsize, cacheFsec, 1, info.BPSector) != 1)
      return 1;
  
  return 0;
}

/** Funkcia vypo��ta �tartovac� klaster z dir entry, (pozn.: Pre FAT12/16 netreba po��ta�, preto�e je vyu��van� max.
  * 16 bitov� hodnota, pri�om vo FAT32 je �tartovac� klaster rozdelen� v �trukt�re do dvoch 16 bitov�ch polo�iek a
  * je potrebn� ich vhodne "spoji�").
  * @param entry �trukt�ra dir polo�ky
  * @return vypo��tan� �tartovac� klaster
  */
unsigned long f32_getStartCluster(F32_DirEntry entry)
{
  return ((unsigned long)entry.startClusterL + ((unsigned long)entry.startClusterH << 16));
}

/** Funkcia nastav� �tartovac� klaster do dir entry
 *  @param cluster ��slo klastra, ktor� sa pou�ije ako nov� hodnota �tart. klastra
 *  @param entry[v�stup] smern�k na �trukt�ru dir polo�ky, do ktorej sa zap�e nov� hodnota �tart.klastra
 */
void f32_setStartCluster(unsigned long cluster, F32_DirEntry *entry)
{
  (*entry).startClusterH = (unsigned short)((unsigned long)(cluster & 0xffff0000) >> 16);
  (*entry).startClusterL = (unsigned short)(cluster & 0xffff);
}

/** Funkcia n�jde �al�� klaster v re�azi (nasledovn�ka predchodcu)
 *  @param cluster ��slo klastra (predchodca)
 *  @return vr�ti hodnotu predchudcu z FAT
 */
unsigned long f32_getNextCluster(unsigned long cluster)
{
  unsigned long val;
  if (f32_readFAT(cluster, &val))
    error(0,gettext("Can't read from FAT !"));
  return val;
}

/** Funkcia na��ta cel� klaster do pam�te (d�ta, nie hodnotu FAT), potrebn� �daje berie z u� naplnenej �trukt�ry F32_info
 *  (ako napr. sectors per cluster, at�).
 *  @param cluster ��slo klastra, ktor� sa m� na��ta�
 *  @param buffer[v�stup] smern�k na buffer, kde sa cluster na��ta
 *  @return v pr�pade chyby vr�ti 1, inak 0
 */
int f32_readCluster(unsigned long cluster, void *buffer)
{
  unsigned long logicalLBA;
  if (!f32_mounted()) return 1;
  
  if (cluster > info.clusterCount)
    error(0,gettext("Trying to read cluster > max !"));

  logicalLBA = info.firstDataSector + (cluster - 2) * bpb.BPB_SecPerClus;
  if (d_readSectors(logicalLBA, buffer, bpb.BPB_SecPerClus, info.BPSector) != bpb.BPB_SecPerClus)
    return 1;
  else
    return 0;
}

/* zapise cely cluster do pamate */
/** Funkcia zap�e cel� klaster z pam�te do obrazu (d�ta, nie hodnotu FAT), potrebn� �daje berie z u� naplnenej
 *  �trukt�ry F32_info (ako napr. sectors per cluster, at�).
 *  @param cluster ��slo klastra, do ktor�ho sa bude zapisova�
 *  @param buffer smern�k na buffer, z ktor�ho sa bud� ��ta� �daje
 *  @return v pr�pade chyby vr�ti 1, inak 0
 */
int f32_writeCluster(unsigned long cluster, void *buffer)
{
  unsigned long logicalLBA;
  if (!f32_mounted()) return 1;
 
  if (cluster > info.clusterCount)
    error(0,gettext("Trying to write cluster > max !"));
  
  logicalLBA = info.firstDataSector + (cluster - 2) * bpb.BPB_SecPerClus;
  if (d_writeSectors(logicalLBA, buffer, bpb.BPB_SecPerClus, info.BPSector) != bpb.BPB_SecPerClus)
    return 1;
  else
    return 0;
}
