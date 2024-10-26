#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "network.h"
#include "transport.h"

static bool server=false;
Socket_t netsocket=-1;

#define BUFFER_SIZE 32767
static uint8_t *buffer=NULL;

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>
double GetClock(void)
{
	static uint64_t frequency=0;
	uint64_t count;

	if(!frequency)
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);

	QueryPerformanceCounter((LARGE_INTEGER *)&count);

	return (double)count/frequency;
}
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

double GetClock(void)
{
	struct timespec ts;

	if(!clock_gettime(CLOCK_MONOTONIC, &ts))
		return ts.tv_sec+(double)ts.tv_nsec/1000000000.0;

	return 0.0;
}

// kbhit for *nix
int _kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt=oldt;
	newt.c_lflag&=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf=fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf|O_NONBLOCK);

	ch=getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch!=EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}
#endif

char message[]="Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam et tortor vel tellus molestie pellentesque in id est. Proin sollicitudin purus vitae mi blandit imperdiet. Cras ultrices urna vel urna semper convallis. Praesent in orci a dolor euismod iaculis. Sed eget nisl sed augue porta aliquet ac vitae arcu. Nullam in risus non velit egestas iaculis ac eget tortor. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce imperdiet vestibulum lectus a aliquam. Mauris interdum nulla risus, et posuere ipsum condimentum laoreet. Sed facilisis nunc consequat orci ullamcorper ultricies. Maecenas a dolor orci. Praesent sit amet porta nisi."
"Suspendisse laoreet augue non quam scelerisque, sit amet blandit magna pulvinar.Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Sed interdum consectetur tellus non vulputate.Vivamus molestie sit amet lacus a placerat.Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Vivamus vestibulum velit velit, id egestas quam malesuada sit amet.Fusce lacinia est a orci condimentum gravida.Sed mollis neque nec diam condimentum efficitur.Phasellus facilisis condimentum nunc, non ornare odio venenatis vitae.Nam laoreet venenatis arcu, et semper mi molestie quis.Phasellus id urna at orci suscipit elementum vel vitae est.Etiam tempor ornare pharetra.Maecenas feugiat suscipit turpis, et dictum enim volutpat vitae.Integer eu pharetra sapien.Fusce nec luctus urna."
"Fusce vitae elit erat.Maecenas in odio nulla.Duis molestie vulputate pulvinar.Morbi purus lorem, elementum sed magna ut, aliquet tincidunt diam.Curabitur tempus viverra orci, porttitor tempor quam pharetra at.Vestibulum et nunc sem.Fusce tincidunt ullamcorper nisl in tincidunt.Sed in tellus vel ante tempus semper.Nunc fermentum maximus vulputate.Curabitur vulputate at sapien eget ornare.Mauris eget convallis est.Nunc volutpat volutpat condimentum.Curabitur sit amet arcu vel nibh accumsan semper sit amet ut magna."
"Vestibulum nec ligula purus.Mauris sed sollicitudin libero.Pellentesque rhoncus vitae ante non dignissim.Nullam at erat consequat, pharetra dolor vel, porta augue.Donec imperdiet aliquam nunc.Curabitur ut consequat magna, sit amet pellentesque turpis.In eu porttitor urna.Mauris tempus tristique tempor.Aenean sed feugiat lectus.Maecenas sodales in ex ac rutrum."
"Donec eget risus eget ante condimentum gravida et ut lorem.Etiam venenatis mauris quam, in volutpat dolor lacinia eu.Vestibulum dapibus ipsum vitae ante euismod, vel dictum nulla pretium.Pellentesque ac odio eget mauris consequat dictum a at ex.Nunc sem massa, laoreet id vestibulum sit amet, iaculis quis massa.Etiam aliquam consectetur neque maximus gravida.Donec eget eros sit amet elit tristique condimentum.Nam ultricies imperdiet nisl, vel finibus leo scelerisque ut.Nam volutpat vel ante non molestie.Sed volutpat dictum nibh laoreet egestas.Fusce sit amet erat erat."
"Donec tempus turpis non euismod efficitur.Sed eleifend, mauris quis fermentum faucibus, quam metus consequat augue, quis volutpat metus sapien eget tortor.Nunc vulputate viverra neque quis consectetur.Phasellus malesuada ultricies venenatis.Fusce facilisis volutpat massa, eu consectetur quam vulputate a.Morbi fermentum venenatis imperdiet.Sed eget orci ut risus posuere tincidunt id ac erat."
"Interdum et malesuada fames ac ante ipsum primis in faucibus.Nullam sed dignissim metus, ac blandit ipsum.Quisque congue mattis auctor.Etiam facilisis quis dolor quis consequat.Quisque vel orci sed lorem iaculis porta.Etiam in sapien neque.Suspendisse et elit turpis.Sed ac tortor non felis consectetur porta.Quisque vehicula elementum purus ut porta.Phasellus quis porttitor nibh.Proin diam dui, bibendum vitae nibh vel, maximus molestie eros.Suspendisse potenti.Sed fringilla interdum diam, vitae mollis diam consequat eget."
"Mauris sit amet metus a ligula porta finibus.Curabitur malesuada feugiat vulputate.Phasellus egestas convallis urna, ut condimentum eros accumsan non.Nunc fringilla mauris quis lorem cursus, vitae consequat massa vulputate.Suspendisse potenti.Duis non faucibus dui.Praesent a lacus id velit facilisis facilisis ut vel lorem.Suspendisse sit amet tristique lorem."
"Morbi eleifend porttitor mi et accumsan.Curabitur tempor eget tellus eget tempor.Pellentesque ornare risus leo, in blandit lacus semper eget.Etiam semper, mi eu maximus interdum, elit sem tempus leo, vitae varius nulla eros lacinia tortor.Aenean eget ex magna.Praesent ornare varius scelerisque.Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Morbi at ante vel tortor mattis finibus sed eu velit."
"Donec pellentesque non nisi eu laoreet.In tempor dictum nisl, non tincidunt magna.In non posuere felis.Praesent a magna dolor.Pellentesque tincidunt nibh eget diam consectetur, a fermentum dui ullamcorper.Praesent id maximus ipsum.Etiam a consectetur diam.Curabitur id laoreet felis.Curabitur sed ipsum at odio semper suscipit.Phasellus pellentesque odio sapien, vel dapibus mauris molestie sit amet."
"Curabitur auctor turpis vitae nisl euismod, ut sollicitudin dui facilisis.Ut scelerisque hendrerit magna, in semper tortor imperdiet vitae.Ut id hendrerit metus.Nullam varius sem non tristique tempor.Morbi vitae justo et lacus dictum auctor faucibus a leo.Praesent pharetra metus bibendum, aliquam justo sed, condimentum urna.Curabitur vitae magna nec diam finibus porttitor.Duis lectus justo, suscipit eu turpis ut, gravida sodales nulla.Curabitur sapien tellus, volutpat quis molestie vel, efficitur ut orci."
"Nulla et ullamcorper velit.Vestibulum gravida, magna vel fringilla ultrices, lacus mauris ultrices lorem, non rhoncus diam enim ut ligula.Vivamus vehicula faucibus arcu sed dapibus.Nullam semper ullamcorper ligula, et rhoncus nulla dignissim a.Donec vel rhoncus massa.Ut vitae mauris velit.Aenean ut ligula ornare, ullamcorper dui ac, aliquam enim.Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Vestibulum nec imperdiet nulla, sed sagittis odio.Nunc mattis sapien urna, vitae scelerisque nisl tristique vitae.Proin eleifend nec risus at vestibulum.Maecenas pretium semper ipsum a egestas."
"Praesent eu egestas purus.Quisque dapibus auctor venenatis.Fusce suscipit nisl aliquam scelerisque tempor.Morbi sit amet semper lacus, vel viverra odio.Donec metus nisl, fringilla rhoncus mauris vel, commodo dapibus sem.Sed lobortis lacus et mauris viverra, a volutpat justo placerat.Nunc venenatis ex nec purus faucibus venenatis ut sit amet enim.Mauris tristique congue libero sit amet aliquet."
"Quisque vestibulum nulla dictum, aliquet dolor consectetur, vestibulum quam.Nullam laoreet aliquam laoreet.Phasellus viverra tempus nibh vel molestie.Integer sem libero, pretium non accumsan a, congue feugiat massa.Cras nisl justo, malesuada fringilla libero viverra, consequat bibendum lectus.Duis tristique, lacus eu euismod euismod, nisi metus fringilla purus, non placerat elit urna nec ligula.Aenean nec eros cursus, posuere metus ut, tincidunt nibh.Phasellus at sem sit amet nisl pulvinar iaculis.Nulla ultricies eros at nibh blandit lobortis.Sed commodo tellus in pulvinar ullamcorper.Phasellus ultrices justo eu leo sodales placerat."
"Curabitur non hendrerit massa, at interdum purus.Nullam vel laoreet diam, sit amet finibus lorem.Vivamus sapien orci, posuere ut convallis ac, tempor at urna.Duis finibus ut nunc in rhoncus.Cras aliquam mollis varius.Vivamus faucibus, ligula vel consectetur cursus, turpis nibh egestas arcu, lacinia accumsan elit nisi sit amet metus.Phasellus dictum hendrerit sapien, vel rutrum mi rutrum quis."
"Nam faucibus turpis a nisl tempus sodales.Duis tempus facilisis mi.Vivamus eleifend, orci sit amet aliquet pellentesque, felis magna dictum quam, ac tincidunt neque neque sit amet est.Vivamus elementum quis ante vel sodales.Integer dictum convallis ligula.Vestibulum bibendum id libero ut aliquam.Nulla quam ante, rutrum in nunc in, lacinia laoreet risus."
"Mauris tincidunt, diam vel tempor euismod, sapien dolor luctus libero, gravida vulputate enim urna eget est.Nulla eget augue tincidunt, interdum enim vel, venenatis nunc.Aenean semper risus non justo dignissim, vitae rutrum dui scelerisque.Suspendisse potenti.Suspendisse a ipsum et dolor cursus finibus.Etiam et ultricies ligula.Proin eget enim eget nisi gravida laoreet.Nam ultrices volutpat tortor commodo pulvinar.Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; In interdum lectus sit amet nisi placerat sollicitudin non a turpis.Quisque sollicitudin risus porttitor, auctor tortor quis, vulputate dui.Proin ac sem id magna tristique venenatis.Donec nibh dolor, mollis vel massa non, rhoncus rutrum quam.Nulla justo ante, suscipit pharetra nibh fringilla, luctus ultricies velit.Vivamus eget purus viverra, sodales nulla vitae, ultrices magna."
"Pellentesque laoreet mi ac enim cursus, at efficitur mi hendrerit.Curabitur ut libero luctus, molestie neque at, aliquam mi.Aenean rutrum quam at ex commodo rhoncus.Aenean eget pharetra risus.Pellentesque quis venenatis justo, sit amet vehicula nisl.Vivamus ut magna a ipsum pulvinar facilisis.Nulla posuere imperdiet eros, sit amet pulvinar dolor auctor sit amet.Integer ex mi, consectetur at molestie in, porttitor eu libero.Pellentesque eleifend tellus sed egestas hendrerit.Nunc vulputate, velit vitae elementum fermentum, massa felis ornare metus, sit amet pellentesque ipsum leo eget est.Nunc sed tristique libero.Proin consectetur ex quis ipsum lacinia, ultrices commodo justo egestas.Fusce est metus, hendrerit quis faucibus quis, convallis vitae tortor."
"Nullam dui sem, interdum nec mauris vitae, porttitor blandit erat.Suspendisse condimentum scelerisque euismod.Pellentesque sodales tortor in purus dignissim, aliquam suscipit orci blandit.Nam ac elit tempor, bibendum nulla ut, luctus justo.Praesent maximus, ipsum in scelerisque vulputate, sem sapien pulvinar nibh, sed ultricies tellus mauris non dui.Donec vel metus sagittis, ultricies libero vel, eleifend sem.Nam placerat ipsum sed sollicitudin rutrum.Quisque egestas neque varius dignissim iaculis."
"In venenatis purus tellus, rhoncus volutpat ex tincidunt ac.Proin mattis rhoncus est in dictum.Phasellus ultricies lacus eu ipsum rutrum, eu efficitur justo porta.Duis vestibulum, lectus sit amet fringilla sodales, ipsum sem scelerisque felis, non suscipit tellus eros eget velit.Aenean elementum justo lobortis rhoncus eleifend.Cras aliquam auctor vestibulum.Quisque fermentum euismod purus, et semper mauris placerat a.Sed mi neque, luctus sit amet leo a, porta ornare mi.Sed elit leo, dignissim quis lacinia eget, rhoncus id magna.Maecenas id sem volutpat, rhoncus leo id, blandit tellus.Nullam ligula erat, fermentum hendrerit quam eget, venenatis pretium libero.Vivamus sit amet hendrerit turpis.Maecenas facilisis dui in enim condimentum, id auctor libero dapibus.Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas."
"Aliquam a dui quis velit faucibus dictum.Proin vel tempus ex.Mauris vitae mauris leo.Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Donec convallis sem sollicitudin semper vulputate.Vivamus lobortis congue ante sit amet malesuada.Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Vivamus orci elit, gravida sed velit eget, dapibus egestas neque.Ut tincidunt non nulla a congue.Aenean malesuada aliquet sem in iaculis.Sed ullamcorper convallis magna, eu laoreet lacus tincidunt dignissim.Integer quis arcu sit amet dolor vehicula dapibus."
"Fusce ex purus, aliquam semper nibh vel, pharetra bibendum tortor.Sed leo mi, porttitor quis sagittis sit amet, sagittis et sem.Morbi bibendum justo ac risus tempor viverra.Nullam nec nunc eu ligula consectetur tincidunt vel eu ligula.Nam sed nisl risus.Vestibulum dignissim vestibulum urna vitae elementum.Donec ullamcorper ac nulla a egestas.Duis efficitur diam vel ullamcorper ornare.Duis vitae aliquet risus.Phasellus gravida facilisis efficitur.Quisque enim nisl, mattis pulvinar eleifend id, dapibus non nisl.Integer vitae nibh vel diam pulvinar efficitur.Praesent cursus auctor metus, id dignissim velit tristique vitae."
"Curabitur turpis metus, ornare sed tempor at, molestie eu mi.Praesent rutrum pellentesque vulputate.Ut a odio ultricies, cursus nibh vel, dictum tellus.Curabitur pharetra suscipit erat malesuada eleifend.Etiam luctus nisl a elementum dignissim.Pellentesque ultricies diam quis urna bibendum commodo.Mauris non varius purus, ullamcorper malesuada eros.Pellentesque varius lacus ut eros ultrices placerat sed in sem.Duis et lorem eget nulla dignissim fringilla a nec nunc.Fusce rhoncus ante mi, ut aliquam velit aliquam a.Donec quis diam eu erat fermentum iaculis sed in mi.Morbi id malesuada neque, nec ultrices libero.Proin pharetra convallis neque, ac accumsan felis sagittis eget.Aenean vitae lacus mauris."
"Proin facilisis nunc justo, in blandit sapien eleifend in.Sed vel purus tempus, tempor urna nec, condimentum orci.Maecenas et ex dapibus, eleifend massa non, aliquam enim.Integer cursus risus at vestibulum vehicula.Sed mauris risus, dictum a dignissim eget, tempus vitae augue.Pellentesque rhoncus eget lacus pulvinar scelerisque.Nam vitae eros ut nibh malesuada cursus.Maecenas mi neque, tristique ut consectetur quis, pulvinar sit amet ligula.Nulla bibendum, dolor at scelerisque euismod, mi sem posuere erat, in vehicula neque tortor sit amet diam.Suspendisse at sollicitudin sem.Sed a sagittis tortor."
"Nam volutpat consequat nisl eget dignissim.Etiam eget arcu turpis.Nullam suscipit mi sit amet dui congue, quis auctor leo gravida.Proin mattis sem a lectus eleifend, et ornare lorem porttitor.Suspendisse egestas mauris et nibh dapibus, in facilisis dolor rhoncus.Sed eu augue et purus sodales mattis ac et orci.Quisque ac risus dignissim, malesuada urna consequat, scelerisque urna.Praesent aliquam ipsum ex, sit amet malesuada ex laoreet eu."
"Proin vehicula metus id orci aliquam consequat.Nam imperdiet sit amet ligula vel faucibus.Pellentesque tristique egestas magna, eu imperdiet libero malesuada eleifend.Proin sodales ante quam, sed ullamcorper erat porttitor vel.Fusce vestibulum vel dui id venenatis.Nunc arcu felis, vehicula ut elit eu, accumsan interdum eros.Sed ligula purus, interdum quis sollicitudin in, suscipit et est.Etiam turpis mauris, ullamcorper ac sodales faucibus, elementum et magna.Vestibulum leo metus, laoreet et finibus et, scelerisque nec nunc.Sed id semper augue."
"Mauris maximus rutrum tellus, pulvinar aliquet metus condimentum volutpat.Nullam et leo enim.Nunc tincidunt lacinia odio nec laoreet.Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.Ut sollicitudin mi tempus nisi interdum, ac cursus leo rutrum.Ut ullamcorper ipsum ut urna consectetur molestie.Aliquam pellentesque ac ligula et luctus.Aenean viverra mauris sem, id tempor sapien finibus sed.Phasellus quis sollicitudin sem.Sed vel ullamcorper leo.Donec lacinia odio ex, vel placerat nibh tristique et."
"Praesent non lacus nibh.Sed ultrices faucibus blandit.Vivamus vel suscipit tortor.Duis ex tellus, rutrum eu nunc quis, varius lacinia quam.Sed varius erat vel pulvinar accumsan.Etiam a neque quis turpis hendrerit tempus.Mauris mattis pulvinar elit eu pulvinar.Cras egestas ipsum sapien, ut congue dolor interdum vel.Duis aliquam erat sit amet sem pretium accumsan.Proin luctus in eros ac consectetur.Maecenas efficitur felis in mi elementum egestas."
"Phasellus at libero imperdiet, dignissim mauris sed, dapibus nulla.Aenean aliquam, eros gravida accumsan facilisis, nisi nisi vehicula neque, vel viverra orci dolor quis nunc.Nulla dapibus lectus et fermentum accumsan.Nam finibus, nulla nec rutrum efficitur, velit dui convallis eros, eget interdum magna leo a diam.Duis a quam pellentesque, gravida arcu nec, interdum sapien.Aliquam semper justo vitae arcu volutpat efficitur.Nam molestie lorem et enim pulvinar iaculis.Pellentesque bibendum tortor nec porta vestibulum.Sed eget mi quam.In feugiat ultrices velit, id hendrerit leo placerat quis.Nullam quam lacus, pellentesque vel nisl tristique, hendrerit porta nulla.Aliquam et massa id augue feugiat vestibulum a nec est."
"Morbi vitae lacus sit amet neque sagittis accumsan.Duis justo mauris, lobortis at blandit sed, aliquam ut elit.Phasellus vestibulum magna enim, a luctus est dapibus id.In placerat, tellus eu varius lobortis, ex diam hendrerit risus, sed semper quam ligula eget mauris.Morbi id dapibus augue, in aliquam tortor.Nullam cursus nibh eget est tempus, sit amet vehicula libero mattis.Proin fringilla lectus eu quam tristique feugiat.Duis fringilla lobortis leo, eget ullamcorper urna molestie ut.Sed eu ex turpis.Lorem ipsum dolor sit amet, consectetur adipiscing elit.Nunc imperdiet nunc ac nibh auctor euismod."
"Vivamus quis dapibus massa.Etiam aliquet purus vitae dui maximus, id fringilla diam fringilla.Suspendisse vitae ipsum sit amet sem convallis facilisis non sed urna.Cras sed sapien pharetra, bibendum velit sit amet, aliquam enim.Phasellus quis dapibus metus.Nam consequat molestie elit.Proin non diam sit amet est tincidunt finibus.Nullam placerat turpis diam, aliquet feugiat nibh dictum quis.Suspendisse convallis dictum luctus.Etiam feugiat augue quis arcu laoreet, vel suscipit libero iaculis.Morbi fringilla sit amet nibh ac hendrerit.Morbi sem purus, accumsan vitae ligula vel, auctor pretium velit.Praesent eget eleifend felis, et egestas urna.Quisque vitae consectetur elit."
"Donec vel efficitur justo.Quisque ut lobortis mauris.Curabitur ornare iaculis felis sit amet dignissim.Nulla sodales posuere eros, in condimentum arcu.Suspendisse interdum arcu ante, lacinia maximus neque fringilla sed.In tristique arcu magna, id lacinia eros fermentum id.Nullam quis nisl eu nibh sodales facilisis sed non nisl.Donec vitae accumsan dolor.Phasellus venenatis tristique consectetur.Morbi viverra vestibulum ex condimentum rutrum.In ac lorem nec justo rhoncus interdum.Fusce sit amet accumsan arcu."
"Vivamus id sem pharetra, eleifend augue commodo, pharetra massa.Duis pretium volutpat efficitur.Phasellus ullamcorper leo nec nulla venenatis sagittis.In semper erat quis felis tincidunt ullamcorper.Nullam in nisi et quam placerat ultricies nec vel nulla.Donec in pellentesque urna.Vivamus non purus non odio tempor fringilla sed a elit.Nulla et rutrum ipsum, et tincidunt sem.Aliquam ornare sed justo eget malesuada.Pellentesque ac neque at odio imperdiet fermentum.Ut vitae risus nisi.Ut convallis eu dui non ullamcorper.Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.Praesent leo justo, suscipit non lorem vitae, semper dictum ex."
"Etiam gravida tempus tellus, nec venenatis neque dictum in.Proin condimentum eu sapien imperdiet ornare.Donec sit amet turpis eu purus efficitur dictum eu et magna.Quisque venenatis, dui non scelerisque scelerisque, urna ligula finibus diam, sit amet auctor nunc nisi non dolor.Donec eget euismod arcu.Duis finibus ipsum felis, eu vehicula orci maximus at.Nunc id odio et neque convallis dapibus in eget ante.Sed pellentesque arcu ex, sit amet ullamcorper sapien sodales sit amet.In a massa et ex suscipit venenatis.Duis sed ex elit.Nam eget facilisis risus.In efficitur ullamcorper quam, dapibus condimentum elit rutrum vel.Morbi justo turpis, mattis ac ullamcorper vitae, tempus vitae magna.Quisque ac mi sed tellus fringilla rutrum."
"Aenean vel mattis ante, nec porta leo.Sed id est volutpat, iaculis sapien sit amet, auctor risus.Integer egestas velit id magna feugiat, vitae efficitur odio ultricies.Pellentesque semper quis lorem id consequat.Suspendisse elementum ex et eros pretium, convallis auctor augue vestibulum.Phasellus quis gravida dolor.Integer sit amet pharetra nisl.Vestibulum eu placerat neque.Suspendisse nec dolor dapibus, egestas nisl sit amet, suscipit mauris.In id venenatis elit.Donec efficitur molestie enim.Mauris volutpat lectus at urna commodo, eu dictum elit suscipit.Duis enim nisl, bibendum eget dolor ut, sollicitudin facilisis ligula.Nunc consectetur, lacus non euismod sagittis, tortor augue convallis ligula, vel fermentum ipsum nulla at mauris.Proin non erat quis mauris tempus interdum ac sit amet sapien."
"Proin vestibulum blandit ex ut vulputate.In eget diam quis turpis tincidunt rutrum.Morbi dignissim velit vitae tortor efficitur, nec mollis lacus consequat.Phasellus imperdiet tristique suscipit.Praesent quis ex et purus aliquet aliquet sit amet eu justo.Aenean eu dapibus diam.Sed bibendum congue condimentum.Morbi pretium quam sed felis imperdiet tincidunt.Donec molestie tincidunt orci et congue.Sed at risus tincidunt, bibendum massa sit amet, suscipit tellus.Donec id sodales dui.Donec ut erat ornare, facilisis est a, tristique felis.Curabitur vulputate vestibulum laoreet.Cras ultrices, tellus et suscipit iaculis, risus quam pulvinar justo, eget placerat arcu tortor eget nisi.Donec sollicitudin scelerisque odio, sit amet bibendum quam aliquet sed."
"Integer diam nulla, egestas ac lacus quis, sagittis faucibus ligula.In molestie volutpat ultrices.Etiam pretium augue quis dui pulvinar, eu fringilla lectus sollicitudin.Sed nec laoreet purus.Curabitur facilisis, ligula ac interdum mollis, tortor tellus gravida quam, posuere sagittis metus urna eget risus.Quisque condimentum venenatis mauris, vel ultricies mi pharetra ut.Integer et quam a lectus fringilla rhoncus.Sed nunc quam, accumsan sit amet suscipit tempus, gravida ac mauris.Ut hendrerit nulla vel est porta, in bibendum est accumsan.Etiam ut interdum orci.Fusce porta est in blandit pharetra.Donec sagittis maximus sapien et pretium.Sed quis mi nisl.Suspendisse sed libero luctus sem hendrerit lobortis.Nunc eu neque libero.Mauris non ullamcorper urna."
"Sed ac neque fringilla, dignissim odio quis, pulvinar nisi.In non scelerisque lacus, non vehicula dolor.Suspendisse fringilla imperdiet elit eu convallis.Sed nec scelerisque leo, eget ornare ligula.Pellentesque nec mollis leo, at pellentesque purus.Donec interdum, tortor et blandit vehicula, turpis libero tincidunt quam, eu pellentesque sapien nisi id ipsum.Aenean eu neque vel diam ornare euismod.Sed vehicula tristique faucibus.Vestibulum at eleifend quam."
"Nullam pharetra urna a odio imperdiet molestie.Curabitur eu rhoncus elit.Curabitur ligula metus, ultrices vel convallis eu, ornare non arcu.Integer in erat eget lacus sollicitudin sodales.Maecenas sit amet dui sed purus commodo sagittis.Duis in leo in elit malesuada blandit nec eu urna.In sodales nunc nec vestibulum aliquam.Suspendisse potenti.Interdum et malesuada fames ac ante ipsum primis in faucibus.Proin nec mauris sit amet turpis posuere hendrerit.Suspendisse eget aliquet ante.Sed pulvinar lorem vitae egestas dignissim."
"Nulla efficitur mollis nunc sit amet tincidunt.Vivamus scelerisque fringilla enim, nec pretium risus suscipit eget.Donec lacinia nec felis interdum aliquet.Suspendisse non odio quis ligula mollis hendrerit sit amet vitae lorem.Interdum et malesuada fames ac ante ipsum primis in faucibus.Donec condimentum, enim sit amet venenatis ultricies, neque diam cursus est, at elementum tellus odio sed libero.Aenean massa leo, efficitur quis finibus ut, accumsan ac dui.Praesent sed molestie arcu, vitae elementum elit."
"Sed vel accumsan sapien.Aenean pellentesque ante vel ullamcorper sagittis.Curabitur fermentum luctus sagittis.Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Donec in libero fringilla tortor dictum venenatis sed sed enim.Nulla eu facilisis magna.Fusce nulla ante, ultrices non risus sit amet, fringilla aliquet risus.Aliquam id lacinia lectus, sed aliquet leo.Nulla a urna hendrerit, pulvinar diam ut, vulputate nulla."
"Duis non augue sit amet purus auctor egestas.Etiam malesuada hendrerit ex at iaculis.Donec tincidunt euismod efficitur.Integer ut ante tellus.Sed tempor tincidunt risus vel feugiat.In eget mollis purus, id maximus nulla.Curabitur id pulvinar dui.Nunc nec tempus quam.Curabitur nec porttitor elit.Morbi molestie, lacus ac ornare scelerisque, ante nisi euismod sem, sit amet mollis sapien lectus a augue.Aenean consequat iaculis mi.Integer ac ipsum eget eros interdum placerat.In auctor, ex et scelerisque posuere, arcu neque venenatis nunc, eget rhoncus massa risus ornare sem.Mauris sit amet bibendum nulla, auctor tincidunt sapien."
"Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.Morbi nunc ipsum, aliquam non ipsum id, sagittis tristique odio.Vestibulum eu tellus id turpis luctus auctor.Fusce vulputate purus at aliquet hendrerit.Suspendisse molestie erat purus.Vestibulum ut dapibus dolor.Aenean at elit ac diam ultrices egestas.In ante ligula, laoreet nec luctus dictum, malesuada et elit.Vestibulum eget nunc efficitur, vehicula libero eget, auctor nisi.Fusce eget lacus maximus, aliquam massa sit amet, sagittis metus.Duis mollis suscipit maximus.Morbi posuere augue ullamcorper, rutrum felis at, euismod lorem.Quisque sit amet pellentesque nisi.Vivamus ut sollicitudin eros.Morbi fermentum maximus quam, eget laoreet justo."
"Nullam vehicula sapien sed fringilla eleifend.Praesent sed purus nec lacus semper ultrices.Morbi felis ante, convallis malesuada molestie eget, luctus quis augue.Cras tempor nibh et ante pretium, et congue sem vestibulum.Proin molestie urna metus, efficitur interdum nisl bibendum et.Morbi posuere convallis ornare.Curabitur pulvinar magna porta, luctus ipsum sit amet, faucibus mi.Donec et sodales tellus.Phasellus tempor dui dignissim erat ullamcorper rutrum.Maecenas mattis convallis libero eget tempus.Nunc varius felis eget placerat dictum.Aenean ut tristique arcu.Aenean viverra maximus faucibus.Integer congue ipsum eget fringilla eleifend."
"Phasellus eros justo, porttitor ut urna consequat, interdum blandit dolor.Praesent elementum, erat at ultricies tempus, sem nibh vestibulum leo, at maximus nulla leo sed lacus.Suspendisse sapien enim, feugiat vel efficitur vehicula, porttitor quis magna.Integer rutrum dui enim, ut accumsan ex tristique eget.Vestibulum sit amet arcu ac arcu sollicitudin imperdiet.Fusce quis scelerisque libero.Cras porta urna ac felis finibus vehicula.Etiam id mi sapien.Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.Donec porttitor sed libero cursus varius.Quisque sit amet sodales felis, nec ullamcorper ipsum."
"Quisque vehicula eleifend nibh, ac tempor tortor vehicula sit amet.Nam eleifend eu nibh scelerisque tristique.Sed accumsan lectus mattis rutrum rutrum.Donec ac euismod nisl.Aliquam lectus diam, sodales at nunc eu, interdum pretium nisl.Proin luctus pulvinar lacinia.Nunc nec convallis nisi.Fusce interdum sodales sem, a semper sem sodales in.Maecenas elementum ut turpis eget viverra."
"Phasellus quis tortor egestas, porttitor tellus sed, cursus libero.In fermentum euismod urna, vitae suscipit nisl fermentum gravida.Nunc fermentum, ex non ullamcorper bibendum, velit nibh lobortis massa, eu congue enim tortor vel odio.Quisque sit amet luctus dui.Duis fringilla hendrerit mollis.Nunc non tempus odio.Donec tempor magna sed magna hendrerit, nec ullamcorper eros porta."
"Donec id pellentesque mi, ut faucibus quam.Praesent eros erat, mollis eu fringilla vitae, pharetra quis libero.Quisque quis velit turpis.Sed quis porttitor tortor, id faucibus libero.Integer aliquam ac dui quis porta.Donec in eros pretium, porttitor odio a, facilisis velit.Ut ac nulla eu diam volutpat consectetur.Morbi mattis lectus eu turpis bibendum interdum.Aliquam elementum tellus ac dolor tincidunt, id scelerisque ante ultrices.Proin et dignissim ipsum, nec malesuada tortor.Donec porta lacinia aliquam.In efficitur ullamcorper sodales.Nunc eget ligula sit amet nisi ornare pulvinar."
"Praesent pulvinar commodo risus, ut viverra sapien.Aenean rutrum neque sed metus porta, eu semper lectus auctor.Aliquam quis eros id magna sodales mattis.Etiam semper, magna id sagittis molestie, sapien tortor volutpat dolor, vel dapibus mauris velit ac ante.Aliquam imperdiet sed metus id ultrices.Nullam mollis egestas ex sit amet vestibulum.Nullam odio dui, varius sed nisl id, varius fringilla ante.Nulla turpis diam, pharetra nec faucibus et, convallis in quam.Curabitur eget fermentum massa.Etiam pretium nisl sed suscipit semper.Vestibulum non ex rhoncus justo rutrum interdum quis at enim.Fusce at odio a urna sollicitudin mattis nec in quam.Vestibulum quis metus vitae arcu euismod luctus.Aenean pellentesque eget magna ac rutrum."
"Mauris at nunc diam.Donec commodo nibh ut neque vulputate, eu laoreet tellus ornare.Suspendisse eget mauris erat.Integer non justo dolor.Duis volutpat diam dignissim elementum vehicula.Etiam ut pretium urna, id aliquet massa.Ut eget interdum enim.Nam vehicula ex orci, ac auctor ante luctus eget.Praesent volutpat massa metus, sed pharetra risus eleifend vestibulum.Aenean dictum efficitur turpis, nec condimentum mi.Suspendisse consectetur risus at ipsum bibendum, vel rhoncus lorem congue.Nullam urna lectus, faucibus eu metus vitae, hendrerit feugiat leo.Donec tincidunt enim id arcu fringilla imperdiet.Maecenas consectetur turpis consequat leo consequat congue."
"Aliquam magna nisi, consectetur ac rutrum in, dignissim condimentum erat.Pellentesque eget tortor sed quam maximus viverra vel quis neque.Aliquam sed porta lectus, ac gravida libero.Curabitur lobortis, nisl nec feugiat scelerisque, sem velit congue mi, sed faucibus enim tellus a ipsum.Nulla vel nulla quis magna tristique euismod sed venenatis ex.In mollis sed leo non vehicula.Morbi sollicitudin urna vitae hendrerit dignissim.Donec volutpat aliquet luctus.Vestibulum urna nunc, efficitur vitae faucibus consectetur, semper laoreet ligula."
"Duis ac eleifend lorem.Vestibulum convallis posuere purus, ac egestas nunc convallis luctus.Vestibulum in leo et velit molestie auctor.Sed efficitur et lorem id consequat.Nam viverra aliquet purus, quis mollis velit cursus et.Nulla mollis eros quis nulla auctor consequat.Fusce maximus ipsum quis neque vulputate rhoncus.Duis sed cursus lorem.Fusce sit amet lobortis ante."
"Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.Sed sit amet quam bibendum, pretium metus ut, accumsan neque.Vestibulum vulputate, sapien et mattis porta, urna diam tincidunt diam, sit amet rutrum quam lectus ut erat.Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.Vestibulum non augue lobortis, sollicitudin metus in, elementum purus.Nulla varius porttitor massa, ut bibendum lacus consectetur eget.Donec fringilla sem in nulla blandit feugiat.Nulla vel orci rutrum, tincidunt mi eu, congue lectus.Vestibulum consequat volutpat est, a condimentum justo aliquet vitae."
"Suspendisse pharetra cursus enim sed vulputate.Aliquam erat volutpat.Pellentesque pharetra purus eu mi consectetur fermentum.Sed neque ex, rhoncus nec varius sit amet, laoreet nec neque.Proin a nisi sed urna accumsan cursus.Proin augue purus, tristique a felis sit amet, fermentum semper velit.Suspendisse purus nibh, consequat nec neque at, mollis ornare arcu.In non dapibus mauris.Sed blandit quam id nunc feugiat malesuada.Phasellus a mi dui."
"In eget volutpat justo.Cras sit amet sollicitudin ex, ut blandit leo.Duis leo neque, ultricies ac pharetra ut, malesuada ut diam.Proin semper dignissim facilisis.Pellentesque sit amet magna nisl.Maecenas laoreet leo sed vestibulum aliquam.Fusce pellentesque pulvinar porta.Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.Curabitur gravida, nibh sed laoreet tincidunt, leo ligula commodo tellus, eget fermentum nulla arcu vel leo.Quisque rutrum dapibus consequat.Fusce vitae nunc rhoncus, suscipit sapien eu, eleifend arcu.Nullam nunc est, rhoncus et turpis ut, consectetur accumsan felis.In a risus lorem."
"Donec consectetur interdum justo a condimentum.Mauris sem augue, hendrerit eget mattis id, auctor eu arcu.Sed convallis tempus nisl ac congue.Quisque lobortis ex a consectetur tempus.Phasellus quis ornare diam, lacinia maximus mauris.Fusce laoreet massa orci, vel varius lorem mollis at.Vivamus bibendum posuere mi, eget sagittis tellus ornare in.Fusce vivamus.";


int main(int argc, char **argv)
{
	if(argc>1&&strncmp("-server", argv[1], 7)==0)
	{
		printf("Server mode\n");
		server=true;
	}
	else
		printf("Client mode\n");

	buffer=(uint8_t *)malloc(BUFFER_SIZE);

	if(buffer==NULL)
	{
		printf("Failed to allocate buffer.\n");
		return -1;
	}

	if(!Network_Init())
	{
		printf("Network initialization failed.\n");
		return -1;
	}

	netsocket=Network_CreateSocket();

	if(netsocket!=-1)
		printf("Created socket: %d\n", netsocket);
	else
		return -1;

	if(server)
	{
		if(!Network_SocketBind(netsocket, NETWORK_ADDRESS(0, 0, 0, 0), 12345))
		{
			printf("Failed to bind socket.\n");
			return -1;
		}

		while(!_kbhit())
		{
#if 1
			Transport_t receiver;

			if(Transport_Receive(&receiver))
			{
				printf("Got: ");
				for(uint32_t i=0;i<receiver.length;i++)
					putchar(receiver.buffer[i]);
				putchar('\n');

				free(receiver.buffer);
			}
#else
			uint32_t address=0;
			uint16_t port=0;
			int32_t length=ReliableSocketReceive(netsocket, buffer, BUFFER_SIZE, &address, &port);

			if(length>0)
				printf("Got %d bytes on %d from %X. (%llu)\n", length, port, address, *(uint64_t *)buffer);
#endif
		}
	}
	else
	{
		uint64_t var=0;
		uint32_t count=0;
		double avgTime=0.0;
		bool Done=false;

		while(!Done)
		{
			if(_kbhit())
			{
#ifdef WIN32
				switch(_getch())
#else
				switch(getchar())
#endif
				{
					case '\x1B':
						Done=true;
						break;

					case ' ':
					{
#if 1
						double start=GetClock();
						Transport_Send((uint8_t *)message, strlen(message));
						printf("Took %lfms.                    \r", (GetClock()-start)*1000.0);
#else
						uint32_t address=NETWORK_ADDRESS(127, 0, 0, 1);
						uint16_t port=12345;

						var++;
						memcpy(buffer, &var, sizeof(uint64_t));

						double start=GetClock();

						if(ReliableSocketSend(netsocket, buffer, sizeof(uint64_t), address, port))
							avgTime+=(GetClock()-start)*1000.0;

						if(count++>10)
						{
							printf("Took %lfms.                    \r", avgTime/count);
							count=0;
							avgTime=0.0;
						}
#endif
						break;
					}
				}
			}
		}
	}

	Network_SocketClose(netsocket);
	Network_Destroy();

	free(buffer);

	return 0;
}
