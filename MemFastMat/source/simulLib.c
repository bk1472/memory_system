#include "common.h"
#include "osadap.h"

char *			pENV_List[100];
static char		_env_out [100];

static char * _strpbrk(const char *cs, const char *ct)
{
	const char *sc1, *sc2;

	for (sc1 = cs; *sc1 != '\0'; ++sc1)
	{
		for( sc2 = ct; *sc2 != '\0'; ++sc2)
		{
			if (*sc1 == *sc2)
				return (char *) sc1;
		}
	}

	return NULL;
}

#ifndef _CYG_WIN_
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	return malloc(len);
}

int munmap(void *addr, size_t len)
{
	(void)free(addr);

	return 0;
}
#endif

char * _strsep(char **s, const char *ct)
{
	char *sbegin = *s, *end;

	if (sbegin == NULL)
		return NULL;

	end = _strpbrk(sbegin, (char *)ct);
	if (end)
		*end++ = '\0';
	*s = end;

	return sbegin;
}

char * _getenv(const char *pEnv)
{
	char	buffer[100];
	int		i = 0;

	if(pEnv == NULL)
		return NULL;

	while(pENV_List[i])
	{
		char	*cp1, *cp2;

		strcpy(&buffer[0], (char *)pENV_List[i]);

		cp2 = &buffer[0];
		cp1 = _strsep(&cp2, "=");
		if(cp1 == NULL || cp2 == NULL)
			return NULL;
		if( strcmp(cp1, (char *)pEnv) == 0 )
		{
			_env_out[0] = '\0';
			strcpy(&_env_out[0], cp2);

			//dbgprint("^Y^%s", &_env_out[0]);
			return &_env_out[0];
		}
		i++;
	}

	return NULL;
}
