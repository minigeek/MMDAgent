/*************************************************************************/
/*                                                                       */
/*                  Language Technologies Institute                      */
/*                     Carnegie Mellon University                        */
/*                      Copyright (c) 2010-2011                          */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author:  Alok Parlikar (aup@cs.cmu.edu)                   */
/*               Date:  March 2010                                       */
/*************************************************************************/
/*                                                                       */
/*  Utility for dumping a clustergen voice as a loadable file            */
/*    Should be safe over different address architectures, but cannot    */
/*    yet load from files dumps with different endianness                */
/*************************************************************************/

#include "cst_file.h"
#include "cst_cg.h"
#include "cst_cart.h"
#include "cst_cg_map.h"

/* Write magic string */
static void cst_cg_write_header(cst_file fd)
{
    const char *header = cg_voice_header_string;

    cst_fwrite(fd,header,1,cst_strlen(header)+1);
    cst_fwrite(fd,&cst_endian_loc,sizeof(int),1);  /* for byte order check */

}

static void cst_cg_write_padded(cst_file fd, const void* data, int numbytes)
{
    /* We used to do 4-byte alignments. but that's not necessary now */
    cst_fwrite(fd, &numbytes, sizeof(int), 1);
    cst_fwrite(fd, data, 1, numbytes);
}

/* Write the CG types */
static void cst_cg_write_db_types(cst_file fd, const cst_cg_db* db)
{
    int i,n;
  
    n=0;
    while(db->types[n])
        n++;

    cst_fwrite(fd, &n,sizeof(int),1);

    for (i=0; i<n; i++)
        cst_cg_write_padded(fd, (void*)(db->types[i]), strlen(db->types[i])+1);
}

/* Write the nodes of a cart tree */
static void cst_cg_write_tree_nodes(cst_file fd, const cst_cart_node* nodes)
{
    int num_nodes, i;

    num_nodes=0;
    while(nodes[num_nodes].val != 0)
        num_nodes++;

    cst_fwrite(fd, &num_nodes,sizeof(int),1);
    for (i=0; i<num_nodes; i++)
    {
	cst_fwrite(fd, &nodes[i].feat,sizeof(char),1);
	cst_fwrite(fd, &nodes[i].op,sizeof(char),1);
	cst_fwrite(fd, &nodes[i].no_node,sizeof(short),1);
        cst_fwrite(fd, &nodes[i].val->c.a.type,sizeof(short),1);
        if (nodes[i].val->c.a.type == CST_VAL_TYPE_STRING)
        {
            cst_cg_write_padded(fd, nodes[i].val->c.a.v.vval, 
                                cst_strlen(nodes[i].val->c.a.v.vval)+1);
        }
        else if (nodes[i].val->c.a.type == CST_VAL_TYPE_INT)
        {
            cst_fwrite(fd, &nodes[i].val->c.a.v.ival,sizeof(int),1);
        }
        else if (nodes[i].val->c.a.type == CST_VAL_TYPE_FLOAT)
        {
            cst_fwrite(fd, &nodes[i].val->c.a.v.fval,sizeof(float),1);
        }
        else
        {   /* its not going to work without more code ... */
            cst_fwrite(fd, &nodes[i].val->c.a.v.ival,sizeof(int),1);
        }
    }
}

/* Write the feats table of a cart tree */
static void cst_cg_write_tree_feats(cst_file fd, const char* const* feats)
{
    int i;

    i=0;
    while(feats[i]) i++;
    cst_fwrite(fd, &i,sizeof(int),1);

    for (i=0; feats[i]; i++)
        cst_cg_write_padded(fd, (void*)(feats[i]), strlen(feats[i])+1);
}

/* Write a cart tree */
static void cst_cg_write_tree(cst_file fd, const cst_cart* tree)
{
    cst_cg_write_tree_nodes(fd, tree->rule_table);
    cst_cg_write_tree_feats(fd, tree->feat_table);
}

/* Write an array of cart trees */
static void cst_cg_write_tree_array(cst_file fd, const cst_cart* const * trees)
{
    int i=0;

    while(trees && trees[i]) i++;
    cst_fwrite(fd, &i,sizeof(int),1);

    for (i=0; trees && trees[i]; i++)
        cst_cg_write_tree(fd, trees[i]);
}

/* Write a single dimensional array whose total size is "bytesize" */
static void cst_cg_write_array(cst_file fd, const void* data, int bytesize)
{
    cst_cg_write_padded(fd, data, bytesize);
}

/* Write a two dimensional array, with every unit item's size given */
static void cst_cg_write_2d_array(cst_file fd, void** data, 
                          int rows, int cols, int unitsize)
{
    int i;
    int columnsize = cols*unitsize;

    cst_fwrite(fd, &rows, sizeof(int),1);

    for(i=0;i<rows;i++)
        cst_cg_write_array(fd, data[i], columnsize);
}

/* Write duration stats */
static void cst_cg_write_dur_stats(cst_file fd, const dur_stat* const * ds)
{
    int i;
    int numstats;
    char* s;
    numstats = 0;
    while(ds[numstats])
        numstats++;

    cst_fwrite(fd, &numstats, sizeof(int), 1);

    /* Write the string resources */
    for(i=0;i<numstats;i++)
    {
        cst_fwrite(fd, &ds[i]->mean, sizeof(float), 1);        
        cst_fwrite(fd, &ds[i]->stddev, sizeof(float), 1);        
        s = ds[i]->phone;
        cst_cg_write_padded(fd, s, strlen(s)+1);
    }
}

/* Write phone-states mapping */
static void cst_cg_write_phone_states(cst_file fd, const char* const * const * ps)
{
    int i,j;
    int count;
    int count2;

    count=0;
    while(ps[count]) count++;
    cst_fwrite(fd, &count, sizeof(int),1);

    for(i=0;i<count;i++)
    {
        count2=0;
        while(ps[i][count2]) count2++;
        cst_fwrite(fd, &count2, sizeof(int),1);

        for(j=0;j<count2;j++)
            cst_cg_write_padded(fd, ps[i][j],strlen(ps[i][j])+1);
    }  
}

/* Write a feature that will get loaded into the voice */
static void cst_cg_write_voice_feature(cst_file fd, 
                                    const char* fname, const char* fval)
{
    cst_cg_write_padded(fd, fname, cst_strlen(fname)+1);
    cst_cg_write_padded(fd, fval, cst_strlen(fval)+1);
}

int cst_cg_dump_voice(const cst_voice *v,const cst_string *filename)
{
    cst_file fd;
    const cst_cg_db* db;

    if ((fd = cst_fopen(filename,CST_OPEN_WRITE|CST_OPEN_BINARY)) == NULL)
        return 0;

    if (!feat_present(v->features,"cg_db"))
        return 0;  /* not a CG voice */

    db = val_cg_db(feat_val(v->features,"cg_db"));

    cst_cg_write_header(fd);

    cst_cg_write_voice_feature(fd, "language", 
                          get_param_string(v->features,"language","eng"));
    cst_cg_write_voice_feature(fd, "country", 
                          get_param_string(v->features,"country","USA"));
    cst_cg_write_voice_feature(fd, "age", 
                          get_param_string(v->features,"age","30"));
    cst_cg_write_voice_feature(fd, "gender", 
                          get_param_string(v->features,"gender","unknown"));
    cst_cg_write_voice_feature(fd, "build_data", 
                          get_param_string(v->features,"build_date","unknown"));
    cst_cg_write_voice_feature(fd, "description", 
                          get_param_string(v->features,"description","unknown"));
    cst_cg_write_voice_feature(fd, "end_of_features","end_of_features");

    /* db name */
    cst_cg_write_padded(fd, (void*)db->name, cst_strlen(db->name)+1);
    cst_cg_write_db_types(fd, db);
    
    cst_fwrite(fd,&db->num_types,sizeof(int),1); 
    cst_fwrite(fd,&db->sample_rate,sizeof(int),1); 
    cst_fwrite(fd,&db->f0_mean,sizeof(float),1); 
    cst_fwrite(fd,&db->f0_stddev,sizeof(float),1); 

    cst_cg_write_tree_array(fd,db->f0_trees);
    cst_cg_write_tree_array(fd,db->param_trees0);
    cst_cg_write_tree_array(fd,db->param_trees1);
    cst_cg_write_tree_array(fd,db->param_trees2);
    
    cst_fwrite(fd,&db->spamf0,sizeof(int),1); 
    if (db->spamf0)
    {
        cst_cg_write_tree(fd,db->spamf0_accent_tree);
        cst_cg_write_tree(fd,db->spamf0_phrase_tree);
    }
  
    cst_fwrite(fd,&db->num_channels0,sizeof(int),1); 
    cst_fwrite(fd,&db->num_frames0,sizeof(int),1); 
    cst_cg_write_2d_array(fd, (void *)db->model_vectors0, db->num_frames0, 
                       db->num_channels0, sizeof(unsigned short));

    cst_fwrite(fd,&db->num_channels1,sizeof(int),1); 
    cst_fwrite(fd,&db->num_frames1,sizeof(int),1); 
    cst_cg_write_2d_array(fd, (void *)db->model_vectors1, db->num_frames1, 
                       db->num_channels1, sizeof(unsigned short));

    cst_fwrite(fd,&db->num_channels2,sizeof(int),1); 
    cst_fwrite(fd,&db->num_frames2,sizeof(int),1); 
    cst_cg_write_2d_array(fd, (void *)db->model_vectors2, db->num_frames2, 
                       db->num_channels2, sizeof(unsigned short));

    if (db->spamf0)
    {
        cst_fwrite(fd,&db->num_channels_spamf0_accent,sizeof(int),1); 
        cst_fwrite(fd,&db->num_frames_spamf0_accent,sizeof(int),1); 
        cst_cg_write_2d_array(fd, (void *)db->spamf0_accent_vectors, 
                              db->num_frames_spamf0_accent, 
                              db->num_channels_spamf0_accent, 
                              sizeof(float));
    }
  
    cst_cg_write_array(fd, db->model_min, sizeof(float)*db->num_channels0);
    cst_cg_write_array(fd, db->model_range, sizeof(float)*db->num_channels0);

    cst_fwrite(fd,&db->frame_advance,sizeof(float),1); 

    cst_cg_write_dur_stats(fd, db->dur_stats);
    cst_cg_write_tree(fd, db->dur_cart);

    cst_cg_write_phone_states(fd, db->phone_states);

    cst_fwrite(fd,&db->do_mlpg,sizeof(int),1); 
    cst_cg_write_array(fd, db->dynwin, db->dynwinsize*sizeof(float));
    cst_fwrite(fd,&db->dynwinsize,sizeof(int),1); 
  
    cst_fwrite(fd,&db->mlsa_alpha,sizeof(float),1); 
    cst_fwrite(fd,&db->mlsa_beta,sizeof(float),1); 

    cst_fwrite(fd,&db->multimodel,sizeof(int),1); 
    cst_fwrite(fd,&db->mixed_excitation,sizeof(int),1); 

    cst_fwrite(fd,&db->ME_num,sizeof(int),1); 
    cst_fwrite(fd,&db->ME_order,sizeof(int),1); 
    cst_cg_write_2d_array(fd, (void *)db->me_h, 
                       db->ME_num, db->ME_order, sizeof(double)); 

    cst_fwrite(fd,&db->spamf0,sizeof(int),1); 
    cst_fwrite(fd,&db->gain,sizeof(float),1); 

    cst_fclose(fd);
    return 1;
}
