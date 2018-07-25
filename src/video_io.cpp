#include <R.h>
#include <Rcpp.h>
#include <Rinternals.h>

// This is needed to read data from the ffmpeg pipe
extern "C"
{
    // Connections.h is a C header and uses some C++ reserved keywords
#define private prriii
#define class classs
#include <R_ext/Connections.h>
#undef private
#undef class
}

// This function reads from the video pipe chunks of block_size bytes and inputs them at the right spot in the cimg array.
// [[Rcpp::export]]
SEXP read_video(SEXP vpipe,
		SEXP cimg_array,
		SEXP nframes,
		SEXP width,
		SEXP height,
		SEXP block_size)
{
    Rconnection con = R_GetConnection(vpipe);
    int nf = INTEGER(nframes)[0];
    int wi = INTEGER(width)[0];
    int he = INTEGER(height)[0];
    size_t bsize = INTEGER(block_size)[0];
    size_t total_size = nf * wi * he * 3;
    if(bsize > total_size)
	bsize = total_size;
    unsigned char* buf = R_Calloc(total_size, unsigned char);
    size_t remaining_size = total_size;
    size_t nbread;
    R_xlen_t i = 0;
    R_xlen_t j = 0;
    R_xlen_t t = 0;
    R_xlen_t rgb = 0;
    R_xlen_t cimg_index = 0;
    while(remaining_size)
    {
	if(bsize < remaining_size)
	    bsize = remaining_size;
	nbread = con->read(buf, sizeof(unsigned char), bsize, con);
	if(nbread != bsize)
	{
	    Rcpp::Rcout << "Read " << nbread << " bytes when expecting: " << bsize << " bytes\n";
	    Rf_error("Unexpected number of bytes read");
	}
	
	for(size_t k = 0; k < bsize; k++)
	{
	    cimg_index = i + (j + (rgb * nf + t) * he) * wi;
	    REAL(cimg_array)[cimg_index] = buf[k] / 255.;
	    rgb ++;
	    if(rgb == 3)
	    {
		rgb = 0;
		i ++;
		if(i == wi)
		{
		    i = 0;
		    j ++;
		    if(j == he)
		    {
			j = 0;
			t ++;
		    }
		}
	    }
	}
	remaining_size -= bsize;
    }
    
    R_Free(buf);
    return(cimg_array);
}


// This function converts and feeds the video to ffmpeg on the fly
// [[Rcpp::export]]
SEXP save_video(SEXP vpipe,
		SEXP cimg_array,
		SEXP nframes,
		SEXP width,
		SEXP height,
		SEXP block_size)
{
    Rconnection con = R_GetConnection(vpipe);
    int nf = INTEGER(nframes)[0];
    int wi = INTEGER(width)[0];
    int he = INTEGER(height)[0];
    size_t bsize = INTEGER(block_size)[0];
    size_t total_size = nf * wi * he * 3;
    if(bsize > total_size)
	bsize = total_size;
    unsigned char* buf = R_Calloc(total_size, unsigned char);
    size_t remaining_size = total_size;
    size_t nbwritten;
    R_xlen_t i = 0;
    R_xlen_t j = 0;
    R_xlen_t t = 0;
    R_xlen_t rgb = 0;
    R_xlen_t cimg_index = 0;
    size_t nbbuf;
    while(remaining_size)
    {
	nbbuf = 0;
	if(bsize > remaining_size)
	    bsize = remaining_size;
	for(t = 0; t < nf ; t ++)
	{
	    for(j = 0; j < he; j ++)
	    {
		for(i = 0; i < wi; i ++)
		{
		    for(rgb = 0; rgb < 3; rgb ++)
		    {
			cimg_index = i + (j + (rgb * nf + t) * he) * wi; 
			buf[nbbuf] = REAL(cimg_array)[cimg_index] * 255;
			nbbuf ++;
			if(nbbuf == bsize)
			{
			    nbwritten = con->write(buf, sizeof(unsigned char), bsize, con);
			    if(nbwritten != bsize)
			    {
				Rcpp::Rcout << "Written " << nbwritten << " bytes when expecting: " << bsize << " bytes\n";
				Rf_error("Unexpected number of bytes written");
				
			    }
			    remaining_size -= bsize;
			    nbbuf = 0;
			    if(bsize > remaining_size)
				bsize = remaining_size;
			}
			if(remaining_size == 0)
			    goto donebuf;
		    }
		}
	    }
	}
    }
    
    R_Free(buf);
 donebuf:
    return(cimg_array);
}
