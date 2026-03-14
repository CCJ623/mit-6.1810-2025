#include "kernel/types.h"
#include "user.h"

int main(){
	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, (char *) &i);

	return 0;
}
