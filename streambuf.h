/*
 * Streambuf manip
 */

struct GenericIntBuf {
	u32 buffer_mask;
	int iput;
	int iget;
	int iputback;             // is previous get pointer

	// diagnostics
	int iswrapped;
	int nwrapped;             // # times it has wrapped         
	int hitide;               // max detected samples in stream
	int qpresent;             // current state of the Q
	spinlock_t lock;
	u32 buffers[0];           // MUST BE LAST!
};
#define NSTREAMBUF (1<<7)


struct StreamBuf {
	struct GenericIntBuf def;
	u32 _buffers[NSTREAMBUF];
};


#define NSTATBUF   (1<<4)
struct StatBuf {
	struct GenericIntBuf def;
	u32 _buffers[NSTATBUF];
};

struct AppBuf {
	char* buf;
	size_t count_max;
	size_t count_actual;
};

#define STREAM_MFA_INVALID 0xFFFFFFFFU

#ifdef PGMCOMOUT
static inline void streamDump( struct StreamBuf* stream_buf, char* caller ) {
	PDEBUGL(0)( "%10s p:%d g:%d\n", caller, stream_buf->iput, stream_buf->iget );
}
#else
#define streamDump( a, b )
#endif

static inline void genericIntBufInit( struct GenericIntBuf* buf, int nbuf ){
	buf->iput = buf->iget = buf->iputback = 0;
	buf->buffer_mask = nbuf - 1;
	spin_lock_init(&buf->lock);
}
static inline int genericIntBufInc( struct GenericIntBuf* buf, int ii ){
	return (ii+1)&buf->buffer_mask;
}
static inline int genericIntBufIsFull( struct GenericIntBuf* buf ){
	return genericIntBufInc( buf, buf->iput ) == buf->iputback;
}

static inline int genericIntBufIsEmpty( struct GenericIntBuf* buf ){
	return buf->iget == buf->iput;
}
static inline int genericIntBufPut( struct GenericIntBuf* buf, u32 data ){
	if ( !genericIntBufIsFull( buf ) ){
		int iput;

		streamDump( buf, "streamPut()" );
		buf->buffers[buf->iput] = data;
		iput = genericIntBufInc( buf, buf->iput );
		if ( iput < buf->iput ){
			buf->iswrapped = 1;
			buf->nwrapped++;
		}
		buf->iput = iput;
		return 0;
	}else{
		streamDump( buf, "streamPut()E" );
		return -1;
	}
}
static inline u32 genericIntBufGet( struct GenericIntBuf* buf ){
	if ( !genericIntBufIsEmpty( buf ) ){
		u32 data;
		unsigned long flags;

		streamDump( buf, "streamGet()" );

		spin_lock_irqsave(&buf->lock, flags); 
		{
			data = buf->buffers[buf->iget];
			buf->iputback = buf->iget;
			buf->iget = genericIntBufInc( buf, buf->iget );
		}
		spin_unlock_irqrestore(&buf->lock, flags);
		return data;
	}else{
		streamDump( buf, "streamGet()E" );
		return STREAM_MFA_INVALID;
	}
}
static inline void genericIntBufPutBack( struct GenericIntBuf* buf )
// puts last buffer from streamGet() back on the Q
{
	buf->iget = buf->iputback;
}
static inline int genericIntBufNumEntries( struct GenericIntBuf* buf )
// returns # entries in buffer NB: unguarded!
{
	int entries = 0;
	int iget = buf->iget;

	while ( iget != buf->iput ){
		iget = genericIntBufInc( buf, iget );
		++entries;
	}

	return entries;
}

static inline void streamBufInit( struct StreamBuf* stream_buf ){
	genericIntBufInit( &stream_buf->def, NSTREAMBUF );
}
static inline int streamPut( struct StreamBuf* stream_buf, u32 data ){
	return genericIntBufPut( &stream_buf->def, data );
}
static inline u32 streamGet( struct StreamBuf* stream_buf ){
	return genericIntBufGet( &stream_buf->def );
}
static inline void streamPutBack( struct StreamBuf* stream_buf )
// puts last buffer from streamGet() back on the Q
{
	genericIntBufPutBack( &stream_buf->def );
}
static inline int streamNumEntries( struct StreamBuf* stream_buf )
// returns # entries in buffer NB: expensive!
{
	return genericIntBufNumEntries( &stream_buf->def );
}

static inline int statbufPut( struct StatBuf* stat_buf, u32 data ) {
	return genericIntBufPut( &stat_buf->def, data );
}
static inline u32 statbufGet( struct StatBuf* stat_buf ){
	return genericIntBufGet( &stat_buf->def );
}
static inline int statbufIsEmpty( struct StatBuf* stat_buf ){
	return genericIntBufIsEmpty( &stat_buf->def );
}

static inline void statBufInit( struct StatBuf* buf ){
	genericIntBufInit( &buf->def, NSTATBUF );
}
static inline void appbufInit( struct AppBuf* app_buf ) {
	app_buf->count_max = app_buf->count_actual = 0;
}



