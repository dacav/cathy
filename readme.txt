NAME
	cathy - Automatic sorting and categorization of files

SYNOPSIS
	find ... -print0 |
	cathy [-C comparer] [-e events_log_file] [-H hasher] [-o outdir] [-r]

DESCRIPTION
	I've got several gigabytes of multimedia content in my backup.  It is
	the result of years of taking and receiving photos or videos.

	It is hard to tell what is good and what is garbage, but it is obvious
	that there's a lot of redundant material, since people tend to send
	the same thing over multiple chats, and chat applications are often
	dumb enough to store multiple copies of the same file, using more
	memory than needed.

	Cathy helps in keeping the backup well sorted, to remove duplicates,
	and to create catalogues (e.g. time-based, hash-based, ...) on the
	file system.

	It is a side project, whose purpose is to fix my own backup.  It is
	not guaranteed to be eventually useful for someone who is not me.

OPTIONS
	-C comparer
		Specify a comparison program.  The default is cmp(1).

	-e events_log_file
		Specify an output file for the event log.

	-H hasher
		Specify a checksum program.  The default is sha1sum(1).

	-o outdir
		Specify an output directory.  The default is ".".

	-r
		Actually remove files.  No file is unlinked unless this flag is
		specified.

NOTES
	It is written in C, because C is *the* programming language. :-)
