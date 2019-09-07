#include "compile.h"
#include "system.h"

static uint8_t space[HEAPSIZE];

static int TermGetLine(void *cookie, char *buf, int len);
FILE *fp;

int main(int argc, char *argv[])
{
    System *sys;
    
    if (argc != 2) {
        VM_printf("usage: basic <file>\n");
        return 1;
    }
    
    if (!(fp = fopen(argv[1], "r"))) {
        VM_printf("can't open '%s'\n", argv[1]);
        return 1;
    }
    
    VM_sysinit(argc, argv);

    sys = InitSystem(space, sizeof(space));
    sys->getLine = TermGetLine;
    sys->getLineCookie = NULL;
    
    Compile(sys);
    
    return 0;
}

/* InitCodeBuffer - initialize the code buffer */
void InitCodeBuffer(ParseContext *c)
{
    c->codeBuf = (uint8_t *)c->image->free;
    c->ctop = (uint8_t *)c->image->top;
    c->cptr = c->codeBuf;
}

static int TermGetLine(void *cookie, char *buf, int len)
{
    return fgets(buf, len, fp) != NULL;
}
