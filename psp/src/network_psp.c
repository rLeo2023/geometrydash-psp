/*
network_psp.c - utils/network.c without the network: online levels are not
there on the PSP (yet), every request fails
*/
#include <stddef.h>
#include "../../source/utils/network.h"

int soc_init()
{
	return -1;
}

void soc_exit()
{
}

int get_level_from_id(char **out_data, int id)
{
	(void)id;
	if (out_data)
		*out_data = NULL;
	return -1;
}
