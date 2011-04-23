/**
 * @file disk.c
 *
 * @brief Modul simuluje fyzick� diskov� oper�cie (cez s�bor obrazu).
 *
 * Mojou snahou bolo nap�sa� nez�visl� modul na ovl�danie fyzick�ch diskov�ch oper�ci�, ktor� je mo�n� v
 * bud�cnosti prep�sa� aj pre pou�itie pre re�lny disk bez nutnosti zmien ostatn�ch modulov. S� tu implementovan�
 * niektor� z�kladn� funkcie ako namontovanie disku (ide o priradenie file descriptora), na��tanie niekolk�ch
 * sektorov a z�pis niekolk�ch sektorov. Tieto funkcie s� implementovan� tak, �e sa iba pohybuj� v s�bore danom
 * deskriptorom disk_descriptor a na danej poz�cii urobia pr�slu�n� oper�cie.
 *
 */
/* Modul som za�al p�sa� d�a: 1.11.2006 */

#include <stdio.h>
#include <unistd.h>

#include <disk.h>
#include <entry.h>

/** Deskriptor s�boru obrazu */
int disk_descriptor = 0;

/** Funkcia namontuje obraz disku (�i�e prirad� glob�lnej premennej disk_descriptor parameter
 *  @param image_descriptor Tento parameter sa prirad� do glob�lnej premennej disk_descriptor
 */
int d_mount(int image_descriptor)
{
  disk_descriptor = image_descriptor;
  return 0;
}


/** Odmontovanie obrazu disku, ide o vynulovanie deskriptora */
int d_umount()
{
  disk_descriptor = 0;
  return 0;
}

/** Funkcia zist�, �i je disk namontovan� */
int d_mounted()
{
  if (!disk_descriptor) return 0;
  else return 1;
}

/** Funkcia pre��ta count sektorov z obrazu z logickej LBA adresy do buffera
 *  @param LBAaddress logick� LBA adresa, z ktorej sa m� ��ta�
 *  @param buffer do tohto buffera sa zap�u na��tan� sektory d�t
 *  @param count po�et sektorov, ktor� sa maj� na��ta�
 *  @param BPSector Po�et bytov / sektor
 *  @return vracia po�et skuto�ne na�itan�ch sektorov
 */
unsigned short d_readSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = read(disk_descriptor, buffer, count * BPSector);
  
  return (unsigned short)(size / BPSector);
}

/** Funkcia zap�e count sektorov do obrazu na logick� LBA adresu z buffera
 *  @param LBAaddress logick� LBA adresa, na ktor� sa m� zapisova�
 *  @param buffer z tohto buffera sa bud� d�ta ��ta�
 *  @param count po�et sektorov, ktor� sa maj� zap�sa�
 *  @param BPSector Po�et bytov / sektor
 *  @return vracia po�et skuto�ne zap�san�ch sektorov
 */
unsigned short d_writeSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = write(disk_descriptor, buffer, count * BPSector);

  return (unsigned short)(size / BPSector);
}
