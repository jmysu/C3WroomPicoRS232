#include <QCoreApplication>

//int main(int argc, char *argv[])
//{
//  QCoreApplication a(argc, argv);
//
//  return a.exec();
//}
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <pcap.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <netinet/if_ether.h> /* includes net/ethernet.h */

#include <QElapsedTimer>
QElapsedTimer timer;

#include <QCoreApplication>
#include <QDebug>
#include <pcap.h>

pcap_if *alldevs;
pcap_if_t *d;
char errbuf[PCAP_ERRBUF_SIZE + 1];
void listAllPcaps()
{
	qDebug() << "Version:" << pcap_lib_version();
	if (pcap_findalldevs(&alldevs, errbuf) != 1) {
		QStringList retval;
		for (d = alldevs; d; d = d->next)
			qDebug() << QString(d->name);
	}
}
/*
 * use this to compare MAC address with authorized list, and check RSSI against expected values.
 *
 */


#define CAPTURE_COUNT 100

//frame control field in 802.11, its 1 byte in total
struct FCF {
	unsigned version : 2;
	unsigned type : 2;
	unsigned subtype : 4;
} __attribute__((packed));

enum types_80211 {
	MGNT = 0,
	CTRL = 1,
	DATA = 2
};

enum subtypes_80211 {
	MGNT_PROBE_REQ = 4,
	MGNT_PROBE_RESP = 5,
	MGNT_PROBE_BEACON = 8,
	MGNT_ESPNOW = 0xD0,
};

struct structEspNOW {
	qint64 timeEplapsed;
	QByteArray src;
	QByteArray dst;
	QByteArray ssid;
	int rssi;
};
structEspNOW myEspNOW;

int count = 0;

void
hexdump(const void *d, size_t datalen)
{
	//const uint8_t *data = d;
	uint8_t data[datalen];
	memcpy(data, d, datalen);

	size_t i, j = 0;

	for (i = 0; i < datalen; i += j) {
		printf("%4zu: ", i);
		for (j = 0; j < 16 && i + j < datalen; j++)
			printf("%02x ", data[i + j]);
		while (j++ < 16)
			printf("   ");
		printf("|");
		for (j = 0; j < 16 && i + j < datalen; j++)
			putchar(isprint(data[i + j]) ? data[i + j] : '.');
		printf("|\n");
	}
}

void
print_mac(u_char *mac)
{
	for (int i = 0; i < 6; i++) {
		printf("%02x", mac[i]);
		if (i < 5)
			putchar(':');
	}
}


/*
   packet structure in byte indices (after radiotap):
   0-1		frame ctrl
   2-3		ID / Duration
   4-9		MAC 1, receiver
   10-15 	MAC 2, destination
   16-21	MAC 3, transmitter
   22-23	sequence ctrl
   for RESPONSE (type 5) ---
   24-35	fixed parameters
   36-N	index tagged parameters:
         - index (1 byte), len (1 byte), data
   for REQ (type 4) ---
   24-N	index tagged parameters:
         - index (1 byte), len (1 byte), data

   PROBE REQUESTS DO NOT HAVE FIXED PARAMS

 */


void
dump80211Packet(const u_char *packet, unsigned plen)
{
	int8_t rssi = packet[22];
	packet += 72; // skip the radiotap header (V0 Len:72)
	struct FCF fcf;
	// check subtype in FCF (1 byte)
	memcpy(&fcf, packet, 1); //Frame Control Field

	myEspNOW.timeEplapsed = timer.elapsed();

	myEspNOW.src = QByteArray((char*)packet + 10, 6); //packet[10] src address
	myEspNOW.dst = QByteArray((char*)packet + 4, 6);   //packet[4] dst address
	//if (((fcf.type == MGNT) && (fcf.subtype == MGNT_PROBE_BEACON)) || fcf.type == 0) {    //PROBE REQUESTS DO NOT HAVE FIXED PARAMS
	//qDebug() << fcf.type << ":" << fcf.subtype << "\t";
	myEspNOW.ssid = myEspNOW.ssid.fromRawData((char*)packet + 38, packet[37]);
	myEspNOW.rssi = rssi;

	qDebug() << QString("%02").arg(fcf.subtype) << ":" << myEspNOW.timeEplapsed << myEspNOW.src.toHex().toUpper() << ">" << myEspNOW.dst.toHex().toUpper() << myEspNOW.ssid << myEspNOW.rssi;
	//}
}

/* callback function that is passed to pcap_loop(..) and called each time
 * a packet is recieved

   struct pcap_pkthdr {
   struct timeval ts;  // time stamp
   bpf_u_int32 caplen; // length of portion present
   bpf_u_int32 len;    // length this packet (off wire)
 #ifdef __APPLE__
      char comment[256];
 #endif
   };

 */
void
pkt_callback(u_char *arg, const struct pcap_pkthdr* pkthdr, const u_char*
             packet)
{
	// hexdump(packet, pkthdr->len);
	dump80211Packet(packet, pkthdr->len);
	// fprintf(stdout,"");

	//for (int i = 0; i < (count / 10) + 1 + 10; i++) {
	//	fprintf(stdout, "\b");
	//}
	//fprintf(stdout, " @%d\n", count);

	// if(count == 4)
	//     fprintf(stdout,"Come on baby sayyy you love me!!! ");
	// if(count == 7)
	//     fprintf(stdout,"Tiiimmmeesss!! ");
	//fflush(stdout);
	//count++;
}



int
main(int argc, char **argv)
{
	int i;
	char *dev;
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t* handle;
	const u_char *packet;
	struct pcap_pkthdr hdr;     /* pcap.h */
	struct ether_header *eptr;  /* net/ethernet.h */

	u_char *ptr; /* printing out hardware header info */

	listAllPcaps();

	if (argc < 2) {
		printf("%s <iface>\n", argv[0]);
		exit(1);
	}
	/* grab a device to peak into... */
	dev = argv[1]; //pcap_lookupdev(errbuf);

	if (dev == NULL) {
		printf("error: %s\n", errbuf);
		exit(1);
	}

	printf("\nPcap DEV: %s\n", dev);


	/* open the device for sniffing.

	   pcap_t *pcap_open_live(char *device,int snaplen, int prmisc,int to_ms,
	   char *ebuf)

	   snaplen - maximum size of packets to capture in bytes
	   promisc - set card in promiscuous mode?
	   to_ms   - time to wait for packets in miliseconds before read
	   times out
	   errbuf  - if something happens, place error string here

	   Note if you change "prmisc" param to anything other than zero, you will
	   get all packets your device sees, whether they are intendeed for you or
	   not!! Be sure you know the rules of the network you are running on
	   before you set your card in promiscuous mode!!     */

	#define READ_TIMEOUT_MS 300
	// handle = pcap_open_live(dev,BUFSIZ,0, READ_TIMEOUT_MS,errbuf);
	handle = pcap_create(dev, errbuf);
	if (handle == NULL) {
		printf("error creating pcap iface: %s\n", errbuf);
		exit(1);
	}

	/*
	 * attempts to set device in monitor mode
	 * pcap_set_rfmon()  sets  whether  monitor  mode should be set on a capture handle when  the handle is activated.
	 * If rfmon is non-zero, monitor mode will be set, otherwise it will not be set.
	 */
	if (pcap_can_set_rfmon(handle)) {
		printf("%s can be set in monitor mode! doing it now...\n", dev);
		pcap_set_rfmon(handle, 1);
	}

	pcap_set_promisc(handle, 1); /* Capture packets that are not yours */
	pcap_set_snaplen(handle, 2048); /* Snapshot length */
	pcap_set_timeout(handle, 200); /* Timeout in milliseconds */
	/* activate the interface for packet capturing */
	if (pcap_activate(handle) != 0) {
		printf("error activating %s: %s\n", dev, pcap_geterr(handle));
		exit(1);
	}


	// handle = pcap_open_live(dev,BUFSIZ,0, READ_TIMEOUT_MS,errbuf);

	// if(handle == NULL)
	// {
	//     printf("pcap_open_live(): %s\n",errbuf);
	//     exit(1);
	// }

	/* Setup filter to only capture
	   probe request and probe response (management frames)
	   type mgt subtype probe-req || type mgt subtype probe-resp
	 */
	struct bpf_program filter;

	//if (pcap_compile(handle, &filter, "type mgt subtype 0xd0", 0, 0) == -1) {
	if (pcap_compile(handle, &filter, "type mgt subtype beacon", 0, 0) == -1) {
		printf("Bad filter - %s\n", pcap_geterr(handle));
		return 2;
	}

	if (pcap_setfilter(handle, &filter) == -1) {
		printf("Error setting filter - %s\n", pcap_geterr(handle));
		return 2;
	}
	timer.start();


	pcap_loop(handle, CAPTURE_COUNT, pkt_callback, NULL);
	//printf("capturing packets...\n");

	pcap_close(handle);

	return 0;
}
