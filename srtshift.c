#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const short VERSION = 1;
const char usage[] = "Usage: srtshift [-s S] [-d RANGE] [-n] [-r] [file] [-v]\n";
const char inv_range[] = "Invalid range!\n";

/* A basic linked list of integers. */
typedef struct Intlist Intlist;
struct Intlist
{
	int i;
	Intlist *next;
};

/* Functions & variables for handling the "dellist", linked list of indices to
 * be deleted. */

Intlist *dellist_head = NULL, *ili;
int dellist_size = 0;
void add_to_dellist(const int i)
{
	ili = malloc(sizeof(Intlist));
	ili->i = i;
	ili->next = dellist_head;
	dellist_head = ili;
	++dellist_size;
}

void clean_dellist()
{
	while(dellist_head)
	{
		ili = dellist_head->next;
		free(dellist_head);
		dellist_head = ili;
	}
	/* dellist_size = 0; */
}

char is_on_dellist(const int i)
{
	/* list is sorted! */
	for(ili = dellist_head; ili && ili->i < i; ili = ili->next);
	return ili && ili->i == i;
}


/* Functions for shifting the time strings in .srt files. */

int toint(const char *s) /* convert "00" into 00 */
{
	return 10*(s[0]-'0') + (s[1]-'0');
}
void tostr(char *s, const int i) /* put 00 into string as "00" */
{
	s[0] = i/10 + '0';
	s[1] = i%10 + '0';
}

/* shift a time value "00:00:00,000" in s by shift milliseconds */
void shift_time(char *s, const int shift)
{
	int tmp;
	/* convert string into time in seconds: */
	long time = 1000*(toint(s)*60*60 /* hours */
		+ toint(s+3)*60 /* minutes */
		+ toint(s+6)) /* seconds */
		+ toint(s+9)*10 /* split seconds */
		+ (s[11]-'0') /* 1000ths */
		+ shift;
	if(time < 0)
	{
		fprintf(stderr, "Warning: resulting timestamp is negative; setting to zero!\n");
		time = 0;
	}
	/* Convert back to string */
	tmp = time/(60*60*1000); /* hours*/
	tostr(s, tmp);
	time -= tmp*60*60*1000;
	tmp = time/(60*1000); /* minutes */
	tostr(s+3, tmp);
	time -= tmp*60*1000;
	tostr(s+6, time/1000); /* seconds */
	time %= 1000;
	tostr(s+9, time/10); /* 100ths */
	s[11] = (time%10) + '0'; /* 1000ths */
}


/* General auxiliary stuff: */

static char ch;

inline char skipl(FILE *f) /* Skip a whole line in the input file */
{
	return ((ch = fgetc(f)) != '\n' && ch != EOF);
}


/* Entry point */

int main(int argc, char *argv[])
{
	int shift = 0;
	char doreindex = 0, docount = 1;
	int i, arg_for_del = 0, arg_for_file = 0;
	for(i = 1; i < argc; ++i)
	{
		if(!strcmp(argv[i], "-v"))
			printf("srtshift version %i\n", VERSION);
		else if(!strcmp(argv[i], "-s"))
		{
			if(++i == argc)
			{
				fprintf(stderr, usage);
				return 1;
			}
			shift = atoi(argv[i]);
		}
		else if(!strcmp(argv[i], "-r"))
			doreindex = 1;
		else if(!strcmp(argv[i], "-n"))
			docount = 0;
		else if(!strcmp(argv[i], "-d"))
		{
			if((arg_for_del = ++i) == argc)
			{
				fprintf(stderr, usage);
				return 1;
			}
		}
		/* assume it's a filename */
		else if(arg_for_file)
		{
			fprintf(stderr, usage);
			fprintf(stderr, "Multiple input files?\n");
			return 1;
		}
		else
			arg_for_file = i;
	}

	if(!arg_for_file)
	{
		fprintf(stderr, usage);
		return 1;
	}

	FILE *infile;
	char buffer[1024];
	if(!(infile = fopen(argv[arg_for_file], "r")))
	{
		fprintf(stderr, "Could not read file %s\n", argv[arg_for_file]);
		return 1;
	}

	if(arg_for_del) /* Read deletion range */
	{
		char* cp = argv[arg_for_del];
		char dorange = 0;
		int j;
		while(*cp)
		{
			if(sscanf(cp, "%d", &i) != 1 || i <= 0)
			{
				fprintf(stderr, inv_range);
				clean_dellist();
				return 1;
			}
			/* else have read a number */
			if(dorange)
			{
				if(i < dellist_head->i)
				{
					fprintf(stderr, inv_range);
					clean_dellist();
					return 1;
				}
				for(j = dellist_head->i + 1; j <= i; ++j)
					add_to_dellist(j);
				dorange = 0;
			}
			else
				add_to_dellist(i);
			do ++cp; while((i /= 10)); /* skip what was read */
			if(*cp == ',')
				++cp; /* next */
			else if(*cp == '-')
			{
				++cp;
				dorange = 1;
			}
		} /* while(*cp) */
		/* sort dellist (selection sort) */
		Intlist *ili2;
		int tmp;
		ili = dellist_head;
		for(i = 0; i < dellist_size - 1; ++i)
		{
			ili2 = ili->next;
			for(j = i+1; j < dellist_size; ++j)
			{
				if(ili->i > ili2->i)
				{
					tmp = ili->i;
					ili->i = ili2->i;
					ili2->i = tmp;
				}
				ili2 = ili2->next;
			}
			ili = ili->next;
		}
	}
	if(dellist_head) /* passing anything to -d implies -r */
		doreindex = 1;

	char subread;
	int numwrote = 1;
	i = 1; /* i counts the read indices */
	for(; fgets(buffer, sizeof(buffer), infile); ++i) /* read index number */
	{
		if(is_on_dellist(atoi(buffer)))
		{
			while(skipl(infile)); /* skip times */
			while(skipl(infile))
				while(skipl(infile)); /* skip subtitles (can be multiline) */
			continue;
		}
		/* else handle entry: */
		if(doreindex)
			fprintf(stdout, "%i\n", numwrote);
		else
			fprintf(stdout, "%s", buffer);

		subread = 0;
		if(fgets(buffer, sizeof(buffer), infile)) /* times: 00:00:00,000 --> 00:00:00,000 */
		{
			if(shift != 0)
			{
				shift_time(buffer, shift);
				shift_time(buffer+17, shift); /* 17=strlen("00:00:00,000 --> "); */
			}
			fprintf(stdout, "%s", buffer);

			while(infile && buffer[0] != '\n')
			{
				if(!fgets(buffer, sizeof(buffer), infile)) /* subtitles */
					break;
				subread = 1;
				fprintf(stdout, "%s", buffer);
			}
		}
		if(!subread)
			fprintf(stderr, "Faulty syntax in entry %i\n", i);
		++numwrote;
	} /* loop over indexed subtitles */
	if(docount)
		fprintf(stderr, "Read %i entries, wrote %i entries\n", i-1, numwrote-1);
	
	clean_dellist();

	return 0;
}
