/**
 * @file entry.c
 *
 * @brief Toto je hlavn� modul, rozl�i parametre a spust� defragment�ciu.
 *
 * Najprv sa nastav� textov� dom�na podla nastavenia LOCALE, potom sa rozparsuj� prep�na�e pomocou
 * funkcie getopt_long. V�etky hl�senia programu (okrem chybov�ch a okrem progress baru) sa zapisuj�
 * do �tandardn�ho pr�du (definovan�ho smern�kom output_stream), ktor� je na za�iatku nastaven� na
 * stdout. V pr�pade, ak je pou�it� prep�na� -l (alebo -log_file), tak s� hl�senia presmerovan�
 * do log s�boru, ktor� je dan� ako prv� argument tohto prep�na�a.
 *
 * @section Options Popis prep�na�ov
 * - -h (alebo --help)                     - Zobraz� inform�cie o pou�it� programu
 * - -l logfile (alebo --log_file logfile) - Presmeruje hl�senia programu do s�boru logfile
 * - -x (alebo --xmode)                    - Program pracuje v debug re�ime (vypisuje sa vela dodato�n�ch inform�ci�)
 *
 */
/* Modul som za�al p�sa� d�a: 31.10.2006 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <libintl.h>
#include <locale.h>

#include <version.h>
#include <entry.h>
#include <fat32.h>
#include <analyze.h>
#include <defrag.h>

/** N�zov programu */
const char *program_name;
/** V�stupn� pr�d (bu� stdout, alebo log s�bor) */
FILE *output_stream;

/** �i je pou�it� debug re�im */
int debug_mode = 0;

/** Proced�ra vyp�e hl�senie o pou�it� programu a ukon�� ho 
 *  @param stream definuje pr�d, do ktor�ho sa bude zapisova�
 *  @param exit_code definuje chybov� k�d, ktor�m sa program ukon��
 */
void print_usage(FILE *stream, int exit_code)
{
  fprintf(stream, gettext("Syntax: %s options image_file\n"), program_name);
  fprintf(stream,
		    gettext("  -h  --help			Shows this information\n"
		    "  -l  --log_file nazov_suboru	Set program output to log file\n"
		    "  -x  --xmode			Work in X mode (debug mode)\n"));
  exit(exit_code);
}

/** Vyp�e chybov� hl�senie do stderr a podla nastavenia parametra p_usage
 *  vyp�se help
 *  @param p_usage �i sa m� zavola� funkcia print_usage
 *  @param message Spr�va na vyp�sanie
 */
void error(int p_usage, char *message, ...)
{
  va_list args;
  fprintf(stderr, gettext("\nERROR: "));
  
  va_start(args, message);
  vfprintf(stderr,message, args);
  va_end(args);
  fprintf(stderr,"\n");
  
  if (p_usage)
    print_usage(stderr, 1);
  else exit(1);
}

/** Hlavn� funkcia
 * @brief nastav� dom�nu spr�v, rozparsuje prep�na�e, vykon� anal�zu fragment�cie disku a zavol� funkciu defragmentovania.
 * Dom�na spr�v sa berie z predvolen�ho adres�ra /usr/share/locale, a m� n�zov f32id_loc.
 * Na rozparsovanie prep�na�ov (ako kr�tkych, tak aj dlh�ch a pr�padne ich argumenty) pou��vam funkciu getopt_long, ktor�
 * to urob� za m�a :-)
 *
 * Ak bol nastaven� prep�na� -h (zobraz� pou�itie programu), tak sa �al�ie parametre u� nerozoberaj� (okrem prep�na�a -l)
 * a program po vyp�san� dan�ho hl�senia ukon�� svoju �innos�. V in�ch pr�padoch program pokra�uje anal�zou fragment�cie
 * a samotnou defragment�ciou (v pr�pade potreby). Defragment�cia sa zapne, iba ak je disk fragmentovan� z minim�lne 1%.
 */
int main(int argc, char *argv[])
{
  int next_option; 				/* �al�� prep�na� */
  int image_descriptor = 0;			/* file deskriptor obrazu FS */
  const char *log_filename = NULL;		/* n�zov log s�boru */
  const char* const short_options = "hl:x";	/* re�azec kr�tkych prep�na�ov */

  /* Pole �trukt�r popisuj�ce platn� dlh� prep�na�e */
  const struct option long_options[] = {
    { "help",		0, NULL, 'h' },
    { "log_file",	1, NULL, 'l' },
    { "xmode",		0, NULL, 'x' },
    { NULL,		0, NULL, 0 }		/* Potrebne na konci pola */
  };
  Oflags flags = { 0,0,0,0 };			/* pr�znaky prep�na�ov programu */

  /* Nastav� sa dom�na spr�v */
  setlocale(LC_ALL, "");
  bindtextdomain("f32id_loc", "/usr/share/locale"); /* default je /usr/share/locale */
  textdomain("f32id_loc");

  output_stream = stdout;			/* pre za�iatok je v�stup na obrazovku */
  /* Skuto�n� n�zov programu sa zapam�t�, aby mohol by� pou�it� v r�znych hl�seniach programu */
  program_name = argv[0];
  opterr = 0; /* Potla�enie v�pisu chybov�ch hl�sen� funkcie getopt_long */
  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'h':  /* -h alebo --help */
        flags.f_help = 1;
	break;
      case 'l':  /* -l alebo --log_file */
        log_filename = optarg;
	flags.f_logfile = 1;
        break;
      case 'x':  /* -x alebo --xmode */
        flags.f_xmode = 1;
        break;
      case '?':
        /* Neplatny prepinac */
	error(0,gettext("Wrong option, use -h or --help"));
      case -1:
        break;
      default:
        abort();
    }
  } while (next_option != -1);

  /* V pr�pade pou�itia log s�boru treba presmerova� do neho stdout */
  if (flags.f_logfile)
    if ((output_stream = fopen(log_filename,"w")) == NULL)
      error(0,gettext("Can't open log file: %s"), log_filename);

  fprintf(output_stream, "FAT32 Image Defragmenter v%s,\n(c) Copyright 2006, vbmacher <pjakubco@gmail.com>\n", __F32ID_VERSION__);
  if (flags.f_help)
    print_usage(output_stream, 0);

  if (flags.f_xmode)
    debug_mode = 1;
  else
    debug_mode = 0;

  /* Teraz premenn� optind ukazuje na prv� nie-prep�na�ov� parameter;
   *  mal by nasledova� jeden parameter - n�zov s�boru image-u
   */
  if (optind == argc)
    error(1,gettext("Missing argument - image file"));

  /* pok�si sa otvori� image */
  if ((image_descriptor = open(argv[optind], O_RDWR)) == -1)
    error(0,gettext("Can't open image file (%s)"), argv[optind]);
  
  /* namontovanie obrazu */
  f32_mount(image_descriptor);

  /* anal�za fragment�cie disku */
  an_analyze();

  /* ak je disk fragmentovan� z min. 1% */
  if ((int)diskFragmentation > 0)
    /** samotn� defragment�cia */
    def_defragTable();
  else
    fprintf(output_stream, gettext("Disk doesn't need defragmentation.\n"));
  /* odmontovanie obrazu, uvolnenie pam�te */
  an_freeTable();
  f32_umount();
  
  close(image_descriptor);
}
