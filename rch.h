/*
 * return command handler Q
 */



#define RCH    struct ReturnCommandHandler
 
static inline void rchInsert(struct list_head* head, RCH* new_entry) 
{
//	info( "new %p lock %p LOCK", new_entry, new_entry->lock );
        spin_lock(new_entry->lock);
	list_add(&new_entry->list, head);
	spin_unlock(new_entry->lock);
//	info( "new %p lock %p unlock", new_entry, new_entry->lock );
}

static inline void rchAppend(struct list_head* head, RCH* new_entry)
{
//	info( "new %p lock %p LOCK", new_entry, new_entry->lock );
        spin_lock(new_entry->lock);
	list_add_tail(&new_entry->list, head);
	spin_unlock(new_entry->lock);
//	info( "new %p lock %p unlock", new_entry, new_entry->lock );
}

static inline void rchRemove( RCH* rch )
{
	spin_lock(rch->lock);
	
	dbg(1, "rch %p list_del %p", rch, &rch->list);
	list_del(&rch->list);
	rch->path = 0;
	rch->handler = 0;
	spin_unlock(rch->lock);
}

/** timeout action. */   
static inline int rchRemoveIfNotValid(RCH *rch)
{
	int valid;

	spin_lock(rch->lock);
	valid = rch->rchReturn.valid;
	if (valid != 1){
		dbg(1, "rch %p valid: %d list_del %p", rch, valid, &rch->list);

		// valid == 0 => cleanup on timeout 
		if (valid < 0){
			list_del(&rch->list);
		}
		rch->handler = 0;
	}
	spin_unlock(rch->lock);	
	return valid;
} 

/** init return command handler
 * @param device
 * @param rch = null means init device global rch 
 */
void rchInit(struct Acq200Device* device, RCH* rch);


static inline struct Acq200Path* rchSetSamplesAndGetPath(
	RCH *rch, unsigned id, int nsamples)
{
	struct Acq200Path *path;

	spin_lock(rch->lock);
	if (nsamples == 0xffffffff){
		rch->rchReturn.valid = -1;
	}else{
		rch->rchReturn.start_id = id;
		rch->rchReturn.nbytes = nsamples*2;
		rch->rchReturn.valid = 1;
	}	

	path = rch->path;
	spin_unlock(rch->lock);

	return path;
}

