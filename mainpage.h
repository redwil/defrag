/**
*
* @author (c) Copyright 2006, Peter Jakubco <pjakubco@gmail.com>
*
* @section Introduction Introduction
* The program is used for defragmentation of disk image files with FAT32 file system. It is split into several
* modules, from that each can  work with its "problem domain". These modules cooperate with each other.
* My aim was to hide as many functions as it is possible, in order I would achieve the biggest "encapsulation"
* (even if the C language is not object oriented). It allowed simplier program debuging.
* 
* While writting this project I was thinking on the motto: Keep It Simple Stupid (KISS).
* In case of any questions, please write me on my email pjakubco@gmail.com
*
* <hr>
* @section Compiling Program compiling
* In order to simplify the compilation I have used the make program and I have created Makefile file. Programs are
* written for gcc compiler and there are needed also two programs to be installed: sed and xgettext.
* 
* The xgettext program is used for automatical retrieving of strings that will be localized and therefore multilanguage
* is supproted. Additional translation and creation of binary localization file is needed to perform manually. Currently,
* there are supported 2 languages: English and Slovak.
*
* <hr>
* @section Release Release notes
* 
*        v0.1b (12.11.2006)
*          Fixed all relevant bugs, command line parameters added
*	v0.1a (4.11.2006) 
*	  simple defragmentation, manipulates with single cluster.
*	  Not functioning checking of FAT table, working only with FAT32.
*         Algorithm as in Windows (pushes everything on the beginning)
*
* <hr>
* @section Requirements System requirements
* The software needs any distribution of UNIX/LINUX that contains tools: gcc, make, xgettext, and sed.
*
*/

