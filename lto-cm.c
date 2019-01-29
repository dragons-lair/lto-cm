#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sg_lib.h"
#include "sg_io_linux.h"
#include "lto-cm.h"

struct globalArgs_t {
	int verbose;
	const char* device_name;
	int mem_id;
	int readopt;
	int writeopt;
	char* msg;
} globalArgs;

//---------------Usage--------------
static void usage()
{
    fprintf(stderr, 
          "LTO-3/LTO-4 Medium Access Memory tool for User Medium Text Label\n"
          "Usage: \n"
          "lto-cm [-h] [-v] -f device (-r|-w \"message\") [-m memory_id]\n"
          " where:\n"
          "    -h/?             Display this usage message\n"
          "    -v               Increase verbosity \n"
          "    -f device        The SCSI-Generic (sg) device\n"
          "    -r/w             Select read OR write (defaults to Memory ID 2051==0x0803),\n"
          "                     If \'w\': supply a \"message\" to write (160 bytes)\n"
          "    -m id            Memory ID (base-10 int) to read from or write to\n"
         );
}

//------------------------------WRITE 0803 FUNCTION---------------------------------
int att_0803_write(int fd, char* data){
return att_write(fd, 2051, data);
}

//---------------------------GENERIC WRITE FUNCTION---------------------------------
int att_write(int fd, int mem_id, char* data){
    char ebuff[EBUFF_SZ];
    int param_len = 0;
    int param_type = 0;
    switch (mem_id) {
    case 2051: /* 0x0803 User Medium Text Label */
		param_len = 160;
		param_type = 2;
//        unsigned char Wr_Att[USER_MEDIUM_TEXT_LABEL_SIZE] = {0,0,0,param_len+5,mem_id,lsb_id,2,0,param_len};
	break;
    case 2054: /* 0x0806 Barcode */
		param_len = 32;
		param_type = 1;
//        unsigned char Wr_Att[BARCODE_SIZE] = {0,0,0,param_len+5,mem_id,lsb_id,2,0,param_len};
	break;
    default: /* won't bother decoding other categories */
        snprintf(ebuff, EBUFF_SZ,
                                "SG_READ_ATT: Writing to memory ID 0x%04x not implemented", mem_id);
                perror(ebuff);
                close(fd);
                return -1;
    }
	int lsb_id = mem_id & 0xFF;
        mem_id = mem_id >> 8;
        if(globalArgs.verbose)printf("SG_READ_ATT to msb_id=0x%02x lsb_id=0x%02x\n", mem_id, lsb_id);
        if(globalArgs.verbose)printf("param_len= %03d\n", param_len);
    int WrBlk_len=param_len+9;
    int ok;
    sg_io_hdr_t io_hdr;
    unsigned char WrAttCmdBlk [WRITE_ATT_CMD_LEN] = {0x8D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, param_len+9, 0, 0};
    unsigned char sense_buffer[32];
/* Figure out how to get rid of Magic # 169 below by dynamically allocating memory for Array Wr_Att */
    unsigned char Wr_Att[169] = {0,0,0,param_len+5,mem_id,lsb_id,param_type,0,param_len};
 //   memset(Wr_Att, sizof(WrBlk_len), char);

    if(strlen (data) > param_len ){ printf("ERROR : String must not Exceed %03d Bytes\n", param_len); return -1;}
/* Figure out how to copy ASCII data from memory w/o using String Copy for Barcode Field... See Below */
//    else{memcpy( (char*)&Wr_Att[9], data, sizeof(data) );}
    else{strcpy( (char*)&Wr_Att[9], data );}
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(WrAttCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = param_len+9;
    io_hdr.dxferp = Wr_Att;
    io_hdr.cmdp = WrAttCmdBlk;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 20000;  


    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        if(globalArgs.verbose)perror("SG_WRITE_ATT_XXXX: Inquiry SG_IO ioctl error");
        /* Revisit above and replace XXXX with Variable for Parameter Attribute */
	close(fd);
        return -1;
    }

    ok = 0;
    switch (sg_err_category3(&io_hdr)) {
    case SG_LIB_CAT_CLEAN:
        ok = 1;
        break;
    case SG_LIB_CAT_RECOVERED:
        if(globalArgs.verbose)printf("Recovered error on SG_WRITE_ATT_0x%02x%02x, continuing\n", mem_id, lsb_id);
        ok = 1;
        break;
    default: /* won't bother decoding other categories */
        if(globalArgs.verbose)sg_chk_n_print3("SG_WRITE_ATT_0803 command error", &io_hdr, 1);
        return -1;
    }

    if (ok) { /* output result if it is available */
        if(globalArgs.verbose)printf("SG_WRITE_ATT_0x%02x%02x duration=%u millisecs, resid=%d, msg_status=%d\n",
               mem_id, lsb_id, io_hdr.duration, io_hdr.resid, (int)io_hdr.msg_status);
    }

return 0;
}


//-----------------------------READ 0803 FUNCTION---------------------------------
int att_0803_read(int fd, char* data){
return att_read(fd, 2051, data);
}

//-----------------------------GENERIC READ FUNCTION---------------------------------
int att_read(int fd, int mem_id, char* data){
	int param_len = 0;
    char ebuff[EBUFF_SZ];

    switch (mem_id) {
    case 2051: /* 0x0803 User Medium Text Label */
		param_len = 160;
        break;
    case 2054: /* 0x0806 Barcode */
		param_len = 32;
        break;
    default: /* won't bother decoding other categories */
        snprintf(ebuff, EBUFF_SZ,
				"SG_READ_ATT: Reading from memory ID 0x%04x not implemented", mem_id);
		perror(ebuff);
		close(fd);
		return -1;
    }

	int ok;
	int lsb_id = mem_id & 0xFF;
	mem_id = mem_id >> 8;
	if(globalArgs.verbose)printf("SG_READ_ATT to msb_id=0x%02x lsb_id=0x%02x\n", mem_id, lsb_id);
	if(globalArgs.verbose)printf("param_len= %03d\n", param_len);
	/* The Below array "rAttCmdBlk" is formed in accordance with T-10 SPC-5 READ_ATTRIBUTE data structure */
    unsigned char rAttCmdBlk[READ_ATT_CMD_LEN] = {0x8C, 0x00, 0, 0, 0, 0, 0, 0, mem_id, lsb_id, 0, 0, 0, param_len-1, 0, 0};
    unsigned char inBuff[READ_ATT_REPLY_LEN];
    unsigned char sense_buffer[32];
    sg_io_hdr_t io_hdr;

  
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rAttCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = READ_ATT_REPLY_LEN;
    io_hdr.dxferp = inBuff;
    io_hdr.cmdp = rAttCmdBlk;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 20000;     

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        if(globalArgs.verbose)perror("SG_READ_ATT: Inquiry SG_IO ioctl error");
        close(fd);
        return -1;
    }

    ok = 0;
    switch (sg_err_category3(&io_hdr)) {
    case SG_LIB_CAT_CLEAN:
        ok = 1;
        break;
    case SG_LIB_CAT_RECOVERED:
        if(globalArgs.verbose)printf("Recovered error on SG_READ_ATT, continuing\n");
        ok = 1;
        break;
    default: /* won't bother decoding other categories */
	printf("ERROR : Attribute 0803h doesn't exist yet, perform a write first\n");
        if(globalArgs.verbose)sg_chk_n_print3("SG_READ_ATT command error", &io_hdr, 1);
        return -1;
    }

    if (ok) { /* output result if it is available */

        if(globalArgs.verbose)printf("SG_READ_ATT duration=%u millisecs, resid=%d, msg_status=%d\n",
               io_hdr.duration, io_hdr.resid, (int)io_hdr.msg_status);

	strncpy( data, (char*)&inBuff[9],160 );
 	}

return 0;
}

//----------------------------- MAIN FUNCTION---------------------------------
int main(int argc, char * argv[])
{
    int sg_fd;
    int k,i,l;
    char * file_name = 0;
    char ebuff[EBUFF_SZ];
    char messageout[160] ;
    int c=0;

   globalArgs.verbose=0;
   globalArgs.device_name=NULL;
   globalArgs.mem_id=0;
   globalArgs.readopt=0;
   globalArgs.writeopt=0;
   globalArgs.msg=NULL;

    while (1) {
   
        c = getopt(argc, argv, "f:m:rw:h?v");

        if (c == -1)
            break;

        switch (c) {
        case 'f':
             if ((globalArgs.device_name=(char*)optarg)==NULL) {
 		fprintf(stderr, "ERROR : Specify a device\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
            }
            break;
        case 'm':
 		if ((globalArgs.mem_id=atoi((char *)optarg))==0) {
		fprintf(stderr, "ERROR : Memory ID not Found... Try Again\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
            	}	
            break;
        case 'r':
	    globalArgs.readopt =1;
            break;
            
        case 'w':
	    globalArgs.writeopt =1;
	    if ((globalArgs.msg=(char*)optarg)==NULL) {
	    	fprintf(stderr, "ERROR : Specify a message\n");
	    	usage();
	    	return SG_LIB_SYNTAX_ERROR;
	    }
            break;
              	
        case 'h':
        case '?':
            usage();
            return 0;
  /*      case 'm':
          if ((globalArgs.msg=(char*)optarg)==NULL) {
 		fprintf(stderr, "ERROR : Specify a message\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
            }
            break;*/
        case 'v':
            ++globalArgs.verbose;
            break;
        default:
            fprintf(stderr, "ERROR : Unrecognised option code 0x%x ??\n", c);
            usage();
            return SG_LIB_SYNTAX_ERROR;
        }
        
    }

    if (argc == 1) {
       	usage();
     	return 1;
    }	

	if (optind < argc) {
            for (; optind < argc; ++optind)
                fprintf(stderr, "ERROR : Unexpected extra argument: %s\n",
                        argv[optind]);
            usage();
            return SG_LIB_SYNTAX_ERROR;
        }
        
	if(!(globalArgs.device_name)){
		usage();
                return SG_LIB_SYNTAX_ERROR;
	}
	
	if((globalArgs.readopt)&&(globalArgs.writeopt)){
 		fprintf(stderr, "ERROR : Can't read AND write\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
	}

	if(!((globalArgs.readopt)||(globalArgs.writeopt))){
 		fprintf(stderr, "ERROR : Specify read OR Write operation\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
	}
	if((globalArgs.writeopt)&&(globalArgs.msg==NULL)){
 		fprintf(stderr, "ERROR : Message is missing\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
	}
	
	if((globalArgs.readopt)&&(globalArgs.msg!=NULL)){
 		fprintf(stderr, "ERROR : Unexpected extra argument\n");
		usage();
                return SG_LIB_SYNTAX_ERROR;
	}
 
    if ((sg_fd = open(globalArgs.device_name, O_RDWR)) < 0) {
        snprintf(ebuff, EBUFF_SZ,
                 "ERROR : opening file: %s", file_name);
        perror(ebuff);
        return -1;
    }
    /* Just to be safe, check we have a new sg device by trying an ioctl */
    if ((ioctl(sg_fd, SG_GET_VERSION_NUM, &k) < 0) || (k < 30000)) {
        printf("ERROR :  %s doesn't seem to be an new sg device\n",
               globalArgs.device_name);
        close(sg_fd);
        return -1;
    }


	if (globalArgs.readopt){
		if (globalArgs.mem_id){
			if(att_read(sg_fd, globalArgs.mem_id, messageout)==-1){
				printf("ERROR : Read failed (try verbose opt)\n");
				close(sg_fd);return -1;}
		} else {
			if(att_0803_read(sg_fd,messageout)==-1){
				printf("ERROR : Read failed (try verbose opt)\n");
				close(sg_fd);return -1;}
		}
		l=strlen (messageout);
		for ( i = 0; i < l; ++i ){printf("%c", messageout[i]);}
		printf("\n");
	}
	else if(globalArgs.writeopt){
		if(att_write(sg_fd,globalArgs.mem_id,globalArgs.msg)==-1){
			printf("ERROR : Write failed (try verbose opt)\n");
			close(sg_fd);return -1;}
	}

    else {
    fprintf(stderr, "ERROR : Unavailable Attribute, retry tomorrow\n");
	    return SG_LIB_SYNTAX_ERROR;
	}

    close(sg_fd);
    return 0;
}
