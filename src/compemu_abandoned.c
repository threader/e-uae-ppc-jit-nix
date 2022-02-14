
#if 0   /* some code that was abandoned for one reason or another, but
	   might not yet be ready for complete deletion */

/* The jumps in the following would be a nightmare to get right with the
   new flag handling */

static void readmem_new(int address, int dest, int offset, int size, int tmp)
{
  int f=tmp;
  uae_u8* branchadd;
  uae_u8* branchadd2;
  uae_u8* branchadd3;
  bigstate keep, getto;   /* These are way too big for comfort! */
  int i;

  mov_l_rr(f,address);
  shrl_l_ri(f,16);   /* The index into the baseaddr table */
  mov_l_rm_indexed(f,(uae_u32)baseaddr,f,4);
  /* f now holds either the offset, or whatever is in mem_banks */
  test_l_ri(f,1);  /* Check for LSB */
  keep=live;  /* Store the state for the second branch */
  emit_byte(0x75); branchadd=target; emit_byte(0x00);  /* JNZ */
  { /* Handling realmem situation */
    if (isinreg(dest) && live.state[dest].size>size) {
      tomem(dest); evict(dest);  /* So the state[dest].size will be
				  * just what we loaded --- makes it
				  * easier in the other branch */
    }
    switch(size) {
    case 1: mov_b_rrm_indexed(dest,address,f,1); break;
    case 2: mov_w_rrm_indexed(dest,address,f,1); bswap_16(dest); break;
    case 4: mov_l_rrm_indexed(dest,address,f,1); bswap_32(dest); break;
    }
    if (live.state[dest].size>size) {
      printf("Uh-oh, bad sd in,PC_P readmem, wanted %d\n",
	     live.state[dest].size,size);
      live.state[dest].size=size;
    }
    disassociate(f);
    getto=live;  /* The state we need to get into */
    emit_byte(0xeb); branchadd2=target; emit_byte(0x00); /* JMP */
  }
  if ((uae_u32)target&0x1f) 
    target+=32-((uae_u32)target&0x1f);
  *branchadd=(target-branchadd-1);  /* Fill in the jump length */
  {
    live=keep; /* Restore the pre-jump state */
    mov_l_rR(f,f,-1+offset);  /* -1 makes up for the set LSB */
    /* Now f holds the address of the b/w/lget function */
    isclean(f); /* So it doesn't get pushed */
    call_r_11(f,dest,address,4,4);
    
    /* OK, and now comes the part where we have to make the states the same */
    /* First, the result might have to be moved */
    if (live.state[dest].realreg!=getto.state[dest].realreg) {
      raw_mov_l_rr(getto.state[dest].realreg,live.state[dest].realreg);
      live.nat[getto.state[dest].realreg].holds=dest;
      live.nat[live.state[dest].realreg].holds=-1;
      live.state[dest].realreg=getto.state[dest].realreg;
    }
    /* That was the only live register. Now reload whatever else needs 
       reloading */
    for (i=0;i<N_REGS;i++) {
      if (getto.nat[i].holds>=0 && live.nat[i].holds!=getto.nat[i].holds) {
	raw_mov_l_rm(i,(uae_u32)live.state[getto.nat[i].holds].mem);
      }
    }
    /* Now the state *should* be a superset of the getto state. */
    live=getto;  /* Use the subset */
    emit_byte(0xeb); branchadd3=target; emit_byte(0x00); /* JMP */
  }
  if ((uae_u32)target&0x1f) 
    target+=32-((uae_u32)target&0x1f);
  *branchadd2=(target-branchadd2-1);  /* Fill in the jump length */
  *branchadd3=(target-branchadd3-1);  /* Fill in the jump length */
}


static void writemem_new(int address, int source, int offset, int size, int tmp)
{
  int f=tmp;
  uae_u8* branchadd;
  uae_u8* branchadd2;
  uae_u8* branchadd3;
  bigstate keep, getto;   /* These are way too big for comfort! */
  int i;

  mov_l_rr(f,address);
  shrl_l_ri(f,16);   /* The index into the baseaddr table */
  mov_l_rm_indexed(f,(uae_u32)(baseaddr+65536+512),f,4);
  /* f now holds either the offset, or whatever is in mem_banks */
  test_l_ri(f,1);  /* Check for LSB */
  keep=live;  /* Store the state for the second branch */
  emit_byte(0x75); branchadd=target; emit_byte(0x00);  /* JNZ */
  { /* Handling realmem situation */
    switch(size) {
    case 1: mov_b_mrr_indexed(address,f,1,source); break;
    case 2: bswap_16(source); mov_w_mrr_indexed(address,f,1,source); bswap_16(source); break;
    case 4: bswap_32(source); mov_l_mrr_indexed(address,f,1,source); bswap_32(source); break;
    }
    disassociate(f);
    getto=live;  /* The state we need to get into */
    emit_byte(0xeb); branchadd2=target; emit_byte(0x00); /* JMP */
  }
  if ((uae_u32)target&0x1f) 
    target+=32-((uae_u32)target&0x1f);
  *branchadd=(target-branchadd-1);  /* Fill in the jump length */
  {
    live=keep; /* Restore the pre-jump state */
    mov_l_rR(f,f,-1+offset);  /* -1 makes up for the set LSB */
    /* Now f holds the address of the b/w/lget function */
    isclean(f); /* So it doesn't get pushed */
    call_r_02(f,address,source,4,size);
    
    /* OK, and now comes the part where we have to make the states the same */
    for (i=0;i<N_REGS;i++) {
      if (getto.nat[i].holds>=0 && live.nat[i].holds!=getto.nat[i].holds) {
	raw_mov_l_rm(i,(uae_u32)live.state[getto.nat[i].holds].mem);
      }
    }
    /* Now the state *should* be a superset of the getto state. */
    live=getto;  /* Use the subset */
    emit_byte(0xeb); branchadd3=target; emit_byte(0x00); /* JMP */
  }
  if ((uae_u32)target&0x1f) 
    target+=32-((uae_u32)target&0x1f);
  *branchadd2=(target-branchadd2-1);  /* Fill in the jump length */
  *branchadd3=(target-branchadd3-1);  /* Fill in the jump length */
}


static __inline__ void empty_stack(void)
{
}

static __inline__ void flags_to_stack(void) 
{
  if (live.flags_on_stack==VALID)
    return;
  if (!live.flags_are_important) 
    return;

  if (live.flags_in_flags==VALID) {
    /* Move flags from x86 flags to FLAGTMP */
    int tmp=writereg_specific(FLAGTMP,4,0);
    //lahf(); /* Move low byte into AH. Gets all but V */
    //setcc(tmp,0);  /* move V into bit 0 of AL */
    pushfl();
    raw_pop_l(tmp);

    unlock(tmp);
  }
  else
    abort();
  live.flags_on_stack=VALID;
}


static __inline__ void clobber_flags(void)
{
  if (live.flags_in_flags==VALID && live.flags_on_stack!=VALID)
    flags_to_stack();
  if (live.flags_in_flags!=NADA)
    live.flags_in_flags=TRASH;
}

static __inline__ void live_flags(void)
{
  if (live.flags_on_stack!=NADA)
    live.flags_on_stack=TRASH;
  live.flags_in_flags=VALID;
  live.flags_are_important=1;
}

static __inline__ void dont_care_flags(void)
{
  live.flags_are_important=0;
}

static __inline__ void make_flags_live(void)
{
  if (live.flags_in_flags==VALID)
    return;
  if (live.flags_on_stack==TRASH) {
    printf("Want flags, got something on stack, but it is TRASH\n");
    abort();
  }
  if (live.flags_on_stack==NADA) {
    /* First time --- need to get it from memory */
    push_m((uae_u32)&regflags.cznv);
    pop_l(FLAGTMP);
    live.flags_on_stack=VALID;
  }
  if (live.flags_on_stack==VALID) {

    int tmp=readreg_specific(FLAGTMP,4,0);
    /* AL is 0 (no overflow) or 1 (overflow) */
    //raw_cmp_b_ri(tmp,-127);  /* Set overflow accordingly. Clobber all others */
    //sahf();   /* And set the rest from AH */
    raw_push_l(tmp);
    popfl();

    unlock(tmp);
    live.flags_in_flags=VALID;
    return;
  }
  printf("Huh? live.flags_in_flags=%d, live.flags_on_stack=%d, but need to make live\n",
	 live.flags_in_flags,live.flags_on_stack);
  abort();
}

/* This will swap the nreg s into the nreg d, and
   vice versa, and adjust the state information accordingly */
static void swap_nregs(int s, int d)
{
  int vs=live.nat[s].holds;
  int vd=live.nat[d].holds;

  consistent(__LINE__);
  printf("  Swap %d and %d (%d and %d)\n",s,d,vs,vd);

  if (s==d)
    return;
  if (vs>=0 && vd>=0) 
    raw_xchg_l_rr(s,d);
  else if (vs>=0)
    raw_mov_l_rr(d,s);
  else
    raw_mov_l_rr(s,d);

  if (vs>=0) {
    live.state[vs].realreg=d;
  }
  if (vd>=0) {
    live.state[vd].realreg=s;
  }
  live.nat[s].holds=vd;
  live.nat[d].holds=vs;
  consistent(__LINE__);
}


#endif
