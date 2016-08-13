#include "mzSample.h"
/**
* @file EIC.cpp
* @author Sabu George
* @author Kiran
* @author Sahil
* @version 769
*/
EIC::~EIC() {
        if(spline != NULL) delete[] spline; spline=NULL;
        if(baseline != NULL) delete[] baseline; baseline=NULL;
        peaks.clear();
}

EIC* EIC::eicMerge(const vector<EIC*>& eics) {

        EIC* meic = new EIC();

        unsigned int maxlen = 0;
        float minRt = DBL_MAX;
        float maxRt = DBL_MIN;
        for (unsigned int i=0; i < eics.size(); i++ )  {
                if ( eics[i]->size() > maxlen ) maxlen = eics[i]->size() + 1;
                if ( eics[i]->rtmin  < minRt  ) minRt = eics[i]->rtmin;
                if ( eics[i]->rtmax  > maxRt  ) maxRt = eics[i]->rtmax;
        }

        if (maxlen == 0 ) return meic;

        //create new EIC
        meic->sample = NULL;
        vector<float>intensity(maxlen,0);
        vector<float>rt(maxlen,0);
        vector<int>scans(maxlen,0);
        vector<float>mz(maxlen,0);
        vector<int>mzcount(maxlen,0);

        //smoothing   //initalize time array
        for (unsigned int i=0; i < maxlen; i++ ) {
                rt[i] = minRt + i*((maxRt-minRt)/maxlen); scans[i]=i;
        }

        //combine intensity data from all pulled eics
        for (unsigned int i=0; i< eics.size(); i++ ) {
                EIC* e = eics[i];
                for (unsigned int j=0; j< e->size(); j++ ) {
                        unsigned int bin = ((e->rt[j] - minRt ) / (maxRt-minRt) * maxlen);
                        if (bin >= maxlen) bin=maxlen-1;


                        if(e->spline[j] > 0) {
                                intensity[bin] += e->spline[j];
                        }

                        if(e->mz[j] > 0) {
                                mz[bin] += e->mz[j];
                                mzcount[bin]++;
                        }
                }
        }

        unsigned int eicCount=eics.size();
        for(unsigned int i=0; i<maxlen; i++ ) {
                intensity[i] /= eicCount;
                if (mzcount[i]) mz[i] /=  mzcount[i];
                meic->totalIntensity += intensity[i];
        }

        //copy to new EIC
        meic->rtmin =  minRt;
        meic->rtmax =  maxRt;
        meic->intensity = intensity;
        meic->rt = rt;
        meic->scannum = scans;
        meic->mz = mz;
        return meic;
}

void EIC::computeBaseLine(int smoothing_window, int dropTopX) {

        if (baseline != NULL ) { //delete previous baseline if exists
                delete[] baseline;
                baseline=NULL;
                eic_noNoiseObs=0;
        }

        int n = intensity.size();
        if (n == 0) return;

        try {
                baseline = new float[n];
                std::fill_n(baseline,n,0);
        }  catch(...) {
                cerr << "Exception caught while allocating memory " << n << "floats " << endl;
        }

        //sort intensity vector
        vector<float> tmpv = intensity;
        std::sort(tmpv.begin(),tmpv.end());

        //compute maximum intensity of baseline, any point above this value will
        // be dropped. User specifies quantile of points to keep, for example
        //drop 60% of highest intensities = cut at 40% value;
        float cutvalueF = (100.0-(float) dropTopX)/101;
        unsigned int pos = tmpv.size() * cutvalueF;
        float qcut=0;
        pos < tmpv.size() ? qcut = tmpv[pos] : qcut = tmpv.back();

        //drop all points above maximum baseline value
        for(int i=0; i<n; i++ ) {
                if ( intensity[i] > qcut) {
                        baseline[i]=qcut;
                } else {
                        baseline[i] = intensity[i];
                }
        }

        //smooth baseline
        gaussian1d_smoothing(n,smoothing_window,baseline);

        //count number of observation in EIC above baseline
        for(int i=0; i<n; i++) {
                if(intensity[i]>baseline[i]) eic_noNoiseObs++;
        }
}

void EIC::computeSpline(int smoothWindow) {
        int n = intensity.size();

        if (n == 0) return;
        if ( this->spline != NULL ) { delete[] spline; spline=NULL; }

        try {
                this->spline = new float[n];
                for(int i=0; i<n; i++) spline[i] = 0;
        }  catch(...) {
                cerr << "Exception caught while allocating memory " << n << "floats " << endl;
        }

        //initalize spline, set to intensity vector
        for(int i=0; i<n; i++) spline[i] = intensity[i];

        if (smoothWindow > n/3 ) smoothWindow=n/3;  //smoothing window is too large
        if (smoothWindow <= 1) return;  //nothing to smooth get out

        if( smootherType==SAVGOL) { //SAVGOL SMOOTHER
                mzUtils::SavGolSmoother smoother(smoothWindow,smoothWindow,4);
                vector<float>smoothed = smoother.Smooth(intensity);
                for(int i=0; i<n; i++) spline[i] = smoothed[i];
        } else if (smootherType==GAUSSIAN) { //GAUSSIAN SMOOTHER
                gaussian1d_smoothing(n,smoothWindow,spline);
        } else if ( smootherType == AVG) {
                float* y  = new float[n];
                for(int i=0; i<n; i++) y[i] = intensity[i];
                smoothAverage(y,spline,smoothWindow,n);
                delete[] y;
        }

        /*
           float* x = new float[n];
           float* f = new float[n];
           float* b = new float[n];
           float* c = new float[n];
           float* d = new float[n];

           for(int i=0; i<n; i++) { f[i] = intensity[i]; x[i] = rt[i]; b[i]=c[i]=d[i]=0; }
           mzUtils::cubic_nak(n,x,f,b,c,d);
           for(int i=1; i<n; i++) {
            float dt=0.05;
            spline[i] = f[i-1] + (dt) * ( b[i-1] + ( dt ) * ( c[i-1] + (dt ) * d[i-1] ) );
            //spline[i] = mzUtils::spline_eval(n,x,f,b,c,d,x[i]);
            //cerr << x[i] << " " << f[i] << " " << b[i] << " " << spline[i] << endl;
           }

           delete[] x;
           delete[] f;
           delete[] b;
           delete[] c;
           delete[] d;
         */


}


Peak* EIC::addPeak(int peakPos) {
        peaks.push_back(Peak(this,peakPos));
        return &peaks[peaks.size()-1];
}

void EIC::getPeakPositions(int smoothWindow) {

        //cerr << "getPeakPositions() " << " sWindow=" << smoothWindow << " sType=" << smootherType << endl;

        unsigned int N = intensity.size();
        if ( N == 0 ) return;

        computeSpline(smoothWindow);
        if (spline == NULL ) return;

        for (unsigned int i=1; i < N-1; i++ ) {
                if ( spline[i] > spline[i-1] && spline[i] > spline[i+1]) {
                        addPeak(i);
                } else if ( spline[i] > spline[i-1] && spline[i] == spline[i+1] ) {
                        float highpoint = spline[i];
                        while(i<N-1) {
                                i++;
                                if ( spline[i+1] == highpoint) continue;
                                if ( spline[i+1] > highpoint) break;
                                if ( spline[i+1] < highpoint) { addPeak(i); break; }
                        }
                }
        }

        computeBaseLine(baselineSmoothingWindow, baselineDropTopX);
        getPeakStatistics();
}

void EIC::findPeakBounds(Peak& peak) {
        int apex = peak.pos;

        int ii = apex-1;
        int jj = apex+1;
        int lb = ii;
        int rb = jj;

        unsigned int N = intensity.size();
        if (N==0) return;
        if (!spline) return;
        if (!baseline) return;

        //cerr << "findPeakBounds:" << apex << " " << rt[apex] << endl;

        int directionality = 0;
        float lastValue = spline[apex];
        while(ii > 0 && ii < (int) N) { //walk left
                float relSlope = (spline[ii]-lastValue)/lastValue;
                relSlope > 0.01 ? directionality++ : directionality=0;
                //if (spline[ii]<=spline[lb] ) lb=ii;
                if (intensity[ii]<=intensity[lb] ) lb=ii;
                if (spline[ii] == 0 ) break;
                if (spline[ii]<=baseline[ii]) break;
                if (spline[ii]<=spline[apex]*0.01) break;

                if (directionality >= 2) break;
                lastValue=spline[ii];
                ii=ii-1;
        }

        directionality = 0;
        lastValue = spline[apex];

        while(jj >0 && jj < (int) N ) { //walk right
                float relSlope = (spline[jj]-lastValue)/lastValue;
                relSlope > 0.01 ? directionality++ : directionality=0;
                //if (spline[jj]<=spline[rb] ) rb=jj;
                if (intensity[jj]<=intensity[rb] ) rb=jj;
                if (spline[jj] == 0 ) break;
                if (spline[jj]<=baseline[ii]) break;
                if (spline[jj]<=spline[apex]*0.01) break;

                if (directionality >= 2) break;
                lastValue=spline[jj];
                jj=jj+1;
        }

        //find maximum point in the span from min to max position
        for(unsigned int k=lb; k<rb && k < N; k++ ) {
                if(intensity[k]> intensity[peak.pos] && mz[k] > 0 ) peak.pos = k;
        }

        //remove zero intensity points on the left
        for(unsigned int k=lb; k<peak.pos && k < N; k++ ) {
                if (intensity[k] > 0 ) break;
                lb = k;
        }

        //remove zero intensity points on the right
        for(unsigned int k=rb; k>peak.pos && k < N; k-- ) {
                if (intensity[k] > 0 ) break;
                rb = k;
        }

        //for rare cases where peak is a single observation
        if (lb == apex && lb-1 >= 0) lb=apex-1;
        if (rb == apex && rb+1 < N) rb=apex+1;

        peak.minpos = lb;
        peak.maxpos = rb;
        //cerr << "\tfindPeakBounds:" << lb << " " << rb << " " << rb-lb+1 << endl;
}

void EIC::getPeakDetails(Peak& peak) {
        unsigned int N = intensity.size();

        if (N == 0) return;
        if (baseline == NULL ) return;
        if (peak.pos >= N) return;

        //intensity and mz at the apex of the peaks
        peak.peakIntensity = intensity[ peak.pos ];
        peak.noNoiseObs = 0;
        peak.peakAreaCorrected  = 0;
        peak.peakArea=0;
        float baselineArea=0;
        int jj=0;

        if ( sample != NULL && sample->isBlank ) {
                peak.fromBlankSample = true;
        }

        StatisticsVector<float>allmzs;
        string bitstring;
        if(peak.maxpos >= N) peak.maxpos=N-1;
        if(peak.minpos >= N) peak.minpos=peak.pos;  //unsigned number weirdness.

        float lastValue = intensity[peak.minpos];
        for(unsigned int j=peak.minpos; j<= peak.maxpos; j++ ) {
                peak.peakArea += intensity[j];
                baselineArea +=   baseline[j];
                if (intensity[j] > baseline[j]) peak.noNoiseObs++;

                if(peak.peakIntensity < intensity[j]) {
                        peak.peakIntensity = intensity[j];
                        peak.pos = j;
                }

                if (mz.size() > 0 && mz[j] > 0) allmzs.push_back(mz[j]);

                if (intensity[j] <= baseline[j]) {
                        bitstring += "0";
                } else if (intensity[j] > lastValue) {
                        bitstring += "+";
                } else if( intensity[j] < lastValue) {
                        bitstring += "-";
                } else if( intensity[j] == lastValue) {
                        if (bitstring.length()>1) bitstring += bitstring[bitstring.length()-1]; else bitstring += "0";
                }

                lastValue = intensity[j];
                jj++;
        }

        getPeakWidth(peak);

        if (rt.size()>0 && rt.size() == N ) {
                peak.rt =    rt[ peak.pos ];
                peak.rtmin = rt[ peak.minpos ];
                peak.rtmax = rt[ peak.maxpos ];
        }

        if (scannum.size() && scannum.size() == N ) {
                peak.scan    = scannum[ peak.pos ]; //scan number at the apex of the peak
                peak.minscan = scannum[ peak.minpos ]; //scan number at left most bound
                peak.maxscan = scannum[ peak.maxpos ]; //scan number at the right most bound
        }

        int n =1;
        peak.peakAreaTop = intensity[peak.pos];
        if (peak.pos-1 >= 0 && peak.pos-1 < N)   { peak.peakAreaTop += intensity[peak.pos-1]; n++; }
        if (peak.pos+1 >= 0 && peak.pos+1 < N)   { peak.peakAreaTop += intensity[peak.pos+1]; n++; }

        float maxBaseLine = MAX(MAX(baseline[peak.pos],10), MAX(intensity[peak.minpos], intensity[peak.maxpos]));
        peak.peakMz = mz[ peak.pos ];
        peak.peakAreaTop /= n;
        peak.peakBaseLineLevel = baseline[peak.pos];
        peak.noNoiseFraction = (float) peak.noNoiseObs/(this->eic_noNoiseObs+1);
        peak.peakAreaCorrected = peak.peakArea-baselineArea;
        peak.peakAreaFractional = peak.peakAreaCorrected/(totalIntensity+1);
        peak.signalBaselineRatio = peak.peakIntensity/maxBaseLine;

        if (allmzs.size()> 0 ) {
                peak.medianMz = allmzs.median();
                peak.baseMz =   allmzs.mean();
                peak.mzmin  =   allmzs.minimum();
                peak.mzmax  =   allmzs.maximum();
        }

        if ( peak.medianMz == 0) { peak.medianMz = peak.peakMz; }
        //cerr << peak.peakMz << " " << peak.medianMz << " " << bitstring << endl;


        mzPattern p(bitstring);
        if (peak.width >= 5) peak.symmetry =  p.longestSymmetry('+','-');
        checkGaussianFit(peak);
}



void EIC::getPeakWidth(Peak& peak) {

        int width=1;
        int left=0;
        int right=0;
        unsigned int N = intensity.size();

        for(unsigned int i=peak.pos-1; i>peak.minpos && i < N; i--) {
                if(intensity[i] > baseline[i]) left++; else break;
        }

        for(unsigned int j=peak.pos+1; j<peak.maxpos && j < N; j++ ) {
                if(intensity[j] > baseline[j]) right++; else break;
        }

        peak.width = width + left + right;
}

vector<mzPoint> EIC::getIntensityVector(Peak& peak) {
        vector<mzPoint>y;

        if (intensity.size()>0 ) {
                unsigned int maxi=peak.maxpos;
                unsigned int mini=peak.minpos;
                if(maxi >= intensity.size()) maxi=intensity.size()-1;

                for(unsigned int i=mini; i <= maxi; i++ ) {
                        if(baseline and intensity[i] > baseline[i] )  {
                                y.push_back(mzPoint(rt[i],intensity[i],mz[i]));
                        } else {
                                y.push_back(mzPoint(rt[i],intensity[i],mz[i]));
                        }
                }
        }
        return y;
}


void EIC::checkGaussianFit(Peak& peak) {

        peak.gaussFitSigma=0;
        peak.gaussFitR2 =  0.03;
        int left =  peak.pos - peak.minpos;
        int right = peak.maxpos - peak.pos;
        if (left <= 0 || right <= 0 ) return;
        int moves = min(left, right);
        if (moves < 3 ) return;

        //copy intensities into seperate vector
        //dim
        vector<float>pints(moves*2+1);

        int j=peak.pos+moves; if (j >= intensity.size() ) j=intensity.size()-1; if(j<1) j=1;
        int i=peak.pos-moves; if (i < 1 ) i=1;

        int k=0;
        for(; i<=j; i++) { pints[k]=intensity[i]; k++; }
        mzUtils::gaussFit(pints, &(peak.gaussFitSigma), &(peak.gaussFitR2));
        //cerr << "\tcheckGaussianFit(): Best Sigma=" << peak.gaussFitSigma <<  " minRsqr=" << peak.gaussFitR2 << endl;
}



void EIC::getPeakStatistics() {

        for(unsigned int i=0; i<peaks.size(); i++) {
                findPeakBounds(peaks[i]);
                getPeakDetails(peaks[i]);
        }

        //assign peak ranks based on total area of the peak
        sort(peaks.begin(),peaks.end(),Peak::compArea);
        for(unsigned int i=0; i<peaks.size(); i++) peaks[i].peakRank = i;
}

void EIC::deletePeak(unsigned int i) {
        if (i<peaks.size()) {
                peaks.erase( peaks.begin()+i );
        }
}

void EIC::summary() {
        cerr << "EIC: mz=" << mzmin <<  "-" << mzmax << " rt=" << rtmin << "-" << rtmax << endl;
        cerr << "   : maxIntensity=" << maxIntensity << endl;
        cerr << "   : peaks=" << peaks.size() << endl;
}

void EIC::removeLowRankGroups( vector<PeakGroup>& groups, unsigned int rankLimit ) {
        if (groups.size() < rankLimit ) return;
        std::sort(groups.begin(), groups.end(), PeakGroup::compIntensity);
        for(unsigned int i=0; i < groups.size(); i++ ) {
                if( i > rankLimit ) {
                        groups.erase(groups.begin()+i); i--;
                }
        }
}


vector<PeakGroup> EIC::groupPeaks(vector<EIC*>& eics, int smoothingWindow, float maxRtDiff) {
        //cerr << "EIC::groupPeaks()" << endl;

        //list filled and return by this function
        vector<PeakGroup> pgroups;

        //case there is only a single EIC, there is nothing to group
        if ( eics.size() == 1 && eics[0] != NULL) {
                EIC* m=eics[0];
                for(unsigned int i=0; i< m->peaks.size(); i++ ) {
                        PeakGroup grp;
                        grp.groupId = i;
                        grp.addPeak(m->peaks[i]);
                        grp.groupStatistics();
                        pgroups.push_back(grp);
                }
                return pgroups;
        }

        //create EIC compose from all sample eics
        EIC* m = EIC::eicMerge(eics);
        if (!m) return pgroups;

        //find peaks in merged eic
        m->getPeakPositions(smoothingWindow);

        /*
           cerr << "Peak List" << endl;
           for(unsigned int i=0; i< m->peaks.size(); i++ ) {
           cerr << m->peaks[i]->rtmin << " " << m->peaks[i]->rtmax << " " << m->peaks[i]->peakIntensity << endl;
           }
         */

        for(unsigned int i=0; i< m->peaks.size(); i++ ) {
                PeakGroup grp;
                grp.groupId = i;
                pgroups.push_back(grp);
        }

        //cerr << "EIC::groupPeaks() peakgroups=" << pgroups.size() << endl;

        for(unsigned int i=0; i < eics.size(); i++ ) { //for every sample
                for(unsigned int j=0; j < eics[i]->peaks.size(); j++ ) { //for every peak in the sample
                        Peak& b = eics[i]->peaks[j];
                        b.groupNum=-1;
                        b.groupOverlap=FLT_MIN;

                        //Find best matching group
                        for(unsigned int k=0; k< m->peaks.size(); k++ ) {
                                Peak& a = m->peaks[k];

                                float distx = abs(b.rt-a.rt);
                                float overlap = checkOverlap(a.rtmin,a.rtmax,b.rtmin,b.rtmax); //check for overlap
                                if ( distx > maxRtDiff && overlap < 0.2 ) continue;

                                float disty = abs(b.peakIntensity-a.peakIntensity);
                                //float score= overlap+1/(distx+0.01)+1/(disty+0.01);
                                float score = 1.0/(distx+0.01)/(disty+0.01)*overlap;
                                //Feng note: the new score function above makes sure that the three terms are weighted equally.
                                if ( score > b.groupOverlap) { b.groupNum=k; b.groupOverlap=score; }
                        }

                        /*
                           cerr << b->peakMz <<  " " << b->rtmin << " " << b->rtmax << "->"  << b->groupNum <<
                            " " << b->groupOverlap << endl;
                         */


                        if (b.groupNum != -1 ) {
                                PeakGroup& bestPeakGroup = pgroups[ b.groupNum ];
                                bestPeakGroup.addPeak(b);
                        } else {
                                PeakGroup grp;
                                pgroups.push_back(grp);
                                grp.groupId = pgroups.size()+1;
                                grp.addPeak(b);
                                b.groupOverlap=0;
                        }
                }
        }

        //clean up peakgroup such that there is only one peak for each sample
        for(unsigned int i=0; i< pgroups.size(); i++) {
                PeakGroup& grp = pgroups[i];
                if (grp.peaks.size() > 0 ) {
                        grp.reduce();
                        //grp.fillInPeaks(eics);
                        //Feng note: fillInPeaks is unecessary
                        grp.groupStatistics();
                } else { //empty group..
                        pgroups.erase(pgroups.begin()+i);
                        i--;
                }
        }


        //now merge overlapping groups
        //EIC::mergeOverlapingGroups(pgroups);
        //cerr << "Found " << pgroups.size() << "groups" << endl;

        if(m) delete(m);
        return(pgroups);
}

void EIC::getRTMinMaxPerScan() {
    if ( this->rt.size() > 0 ) {
        this->rtmin = this->rt[0];
        this->rtmax = this->rt[ this->size() - 1];
    }
} 
/**
* This is the functon which gets the EIC of the given scan for the
* given mzmin and mzmax. This function will go through the each scan
* and find the the max intensity and mz corresponding to that max intensity.
* Total intensity is calculated by adding the maxintensity from each scan.
* @param[in] scan This is the 
*/
void EIC::getEICPerScan(Scan* scan, int scanNum, float mzmin,float mzmax) {
    float maxMz = 0, maxIntensity = 0;
    int lb;
    vector<float>::iterator mzItr;

    //binary search
    mzItr = lower_bound(scan->mz.begin(), scan->mz.end(), mzmin);
    lb = mzItr - scan->mz.begin();

    for(unsigned int scanIdx = lb; scanIdx < scan->nobs(); scanIdx++ ) {
        if (scan->mz[scanIdx] < mzmin) continue;
        if (scan->mz[scanIdx] > mzmax) break;
        if (scan->intensity[scanIdx] > maxIntensity ) {
            maxIntensity = scan->intensity[scanIdx];
            maxMz = scan->mz[scanIdx];
        }
    }

    this->scannum.push_back(scanNum);
    this->rt.push_back(scan->rt);
    this->intensity.push_back(maxIntensity);
    this->mz.push_back(maxMz);
    this->totalIntensity += maxIntensity;
    if (maxIntensity > this->maxIntensity) this->maxIntensity = maxIntensity;
}

void EIC::normalizeIntensityPerScan(float scale) {
    if(scale != 1.0) {
        for (unsigned int j = 0; j < this->size(); j++) {
            this->intensity[j] *= scale;
        }
    }

}
