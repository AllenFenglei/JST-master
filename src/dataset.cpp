/**********************************************************************
		        Joint Sentiment-Topic (JST) Model
***********************************************************************

(C) Copyright 2013, Chenghua Lin and Yulan He

Written by: Chenghua Lin, University of Aberdeen, chenghua.lin@abdn.ac.uk.
Part of code is from http://gibbslda.sourceforge.net/.

This file is part of JST implementation.

JST is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

JST is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA

***********************************************************************/

#include "dataset.h"
#include "document.h"
#include "model.h"
#include "map_type.h"
#include "strtokenizer.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdlib.h>
using namespace std; 


dataset::dataset() {
	pdocs = NULL;
	_pdocs = NULL;
	word2atr.clear();
	result_dir = ".";
	wordmapfile = "wordmap.txt";

	numDocs = 0;
	aveDocLength = 0;
	vocabSize = 0;
	corpusSize = 0;
}

dataset::dataset(string result_dir) {
	pdocs = NULL;
	_pdocs = NULL;
	word2atr.clear();
	this->result_dir = result_dir;
	wordmapfile = "wordmap.txt";

	numDocs = 0; 
	aveDocLength = 0;
	vocabSize = 0; 
	corpusSize = 0;
}


dataset::~dataset(void) {
	deallocate();
}


int dataset::read_dataStream(ifstream& fin) {
	string line;
	char buff[BUFF_SIZE_LONG];
	docs.clear();
	numDocs = 0;
	
	while (fin.getline(buff, BUFF_SIZE_LONG)) {
		line = buff;
		if(!line.empty()) {
			// docs ist ein vector und kann damit dynamisch erweitert werden.
			// "getline" liest jedoch nur bis zu einem Zeilenumbruch (\n). Dies entspricht bei unserem Datenformat also genau einem Dokument
			// Es werden hier also alle Trainingsdokumente geladen
			docs.push_back(line);
			numDocs++;
		}
	}
	
	if (numDocs > 0) {
		this->analyzeCorpus(docs);
	}
	
	return 0;
}

// Diese Methode liest alle Dokumente von der .txt ein und legt f�r jedes Wort eine ID + prior-sentilabel an und schreibt dies in die Datenstruktur document (pdocs)
// Diese Fkt. wird nur einmal am Anfang aufgerufen
int dataset::analyzeCorpus(vector<string>& docs) {

	mapword2atr::iterator it;
	mapword2id::iterator vocabIt;   
	mapword2prior::iterator sentiIt;
	map<int,int>::iterator idIt;
		
	string line;
	numDocs = docs.size();
	// Z�hlt die Anzahl an unterschiedlichen Worten/Vokabeln �ber alle Dokumente
	vocabSize = 0;
	// Gibt die Anzahl an allen W�rter in allen Dokumenten an (auch Duplikate)
	corpusSize = 0;
	aveDocLength = 0; 

  // allocate memory for corpus
	if (pdocs) {
		deallocate();
		pdocs = new document*[numDocs];
		// test push to new branch from vs hello
    } 
	else {
		pdocs = new document*[numDocs];
	}
	
	for (int i = 0; i < (int)docs.size(); ++i) {
		line = docs.at(i); // aktuelles Dokument
		//strtokenizer ist eine eigene Klasse des Projekts. Dar�ber k�nnen wir immer wieder auf der aktuellen line (einzelnes Dokument) arbeiten
		strtokenizer strtok(line, " \t\r\n");  // \t\r\n are the separators
		int docLength = strtok.count_tokens();
	
		if (docLength <= 0) {
			printf("Invalid (empty) document!\n");
			deallocate();
			numDocs = vocabSize = 0;
			return 1;
		}
	
		corpusSize += docLength - 1; // the first word is document name/id (z.B. "d0"; deshalb subtrahieren wir 1)
		
		// allocate memory for the new document_i 
		document * pdoc = new document(docLength-1);
		pdoc->docID = strtok.token(0).c_str(); // z.B. "d0"

		// generate ID for the tokens in the corpus, and assign each word token with the corresponding vocabulary ID.
		for (int k = 0; k < docLength-1; k++) {
			int priorSenti = -1;
			// Der Map-Iterator springt an die Stelle, wo dieser Token (Wort) vorkommt
			it = word2atr.find(strtok.token(k+1).c_str());
		
			if (it == word2atr.end()) { //  i.e., new word; denn .find liefert nur .end() zur�ck, wenn der Token nicht gefunden wurde
				pdoc->words[k] = word2atr.size(); //(weil es ein neues Wort ist wissen wir, dass es ganz ans Ende muss)
				sentiIt = sentiLex.find(strtok.token(k+1).c_str()); // check whether the word token can be found in the sentiment lexicon
				// incorporate sentiment lexicon
				if (sentiIt != sentiLex.end()) {
					// Wenn das Wort also im Sentilexicon (mpqa o.�.) vorkommt, dann setze f�r das Wort den entspr. Senti-Prior-Label aus diesem Lexicon
				    priorSenti = sentiIt->second.id;
				}
					
				// insert sentiment info into word2atr
				Word_atr temp = {word2atr.size(), priorSenti};  // vocabulary index (weil es ein neues Wort ist wissen wir, dass es ganz ans Ende muss); word polarity
				word2atr.insert(pair<string, Word_atr>(strtok.token(k+1), temp));
				pdoc->priorSentiLabels[k] = priorSenti;
				
			} 
			else { // word seen before
				// Beachte: Dabei wird diese Funktion (analyzeCorpus) nur einmal am Anfang aufgerufen (in read_dataStream). Sp�ter k�nnen gleiche Worte auch unterschiedliche Labels bekommen
				pdoc->words[k] = it->second.id; // Das Wort ist also schon mit Index bekannt (it->second.id)
				pdoc->priorSentiLabels[k] = it->second.polarity; // Auch die Polarit�t/Sentiment wurde bereits ermittelt
			}
		}
		// W�hrend die Variable "docs" also immer noch sehr nah bei dem rohen Input war, �bertragen wir dies nun in die statische Datenstruktur document (pdoc)
		add_doc(pdoc, i);
	} 
	    
	    
	// update number of words
	vocabSize = word2atr.size();
	aveDocLength = corpusSize/numDocs;

	if (write_wordmap(result_dir + wordmapfile, word2atr)) {
		printf("ERROR! Can not write wordmap file %s!\n", wordmapfile.c_str());
		return 1;
	}
	if (read_wordmap(result_dir + wordmapfile, id2word)) {
		printf("ERROR! Can not read wordmap file %s!\n", wordmapfile.c_str());
		return 1;
	}

	docs.clear();
	return 0;
}



void dataset::deallocate() 
{
	if (pdocs) {
		for (int i = 0; i < numDocs; i++) 
			delete pdocs[i];		
		delete [] pdocs;
		pdocs = NULL;
	}
	
	if (_pdocs) {
		for (int i = 0; i < numDocs; i++) 
			delete _pdocs[i];
		delete [] _pdocs;
		_pdocs = NULL;
	}
}
    

void dataset::add_doc(document * doc, int idx) {
    if (0 <= idx && idx < numDocs)
        pdocs[idx] = doc;
}   

void dataset::_add_doc(document * doc, int idx) {
    if (0 <= idx && idx < numDocs) {
	    _pdocs[idx] = doc;
    }
}

// Hier liest man im Grunde nur das Textfile der Sentiments ein (mpqa)
int dataset::read_senti_lexicon(string sentiLexiconFile) {
	sentiLex.clear();
	char buff[BUFF_SIZE_SHORT];
    string line;
    vector<double> wordPrior;
    int labID;
    double tmp, val;
    int numSentiLabs;
    
    FILE * fin = fopen(sentiLexiconFile.c_str(), "r");
    if (!fin) {
		printf("Cannot read file %s!\n", sentiLexiconFile.c_str());
		return 1;
    }    
    
	// Die while Schleife l�uft �ber das ganze mpqa. Zeile f�r Zeile wird eingelesen
    while (fgets(buff, BUFF_SIZE_SHORT - 1, fin) != NULL) {
		line = buff;
		strtokenizer strtok(line, " \t\r\n");
			
		if (strtok.count_tokens() < 1)  {
			printf("Warning! The strtok count in the lexicon line [%s] is smaller than 2!\n", line.c_str());
		}
		else {	
			tmp = 0.0;
			labID = 0;
			wordPrior.clear();
			numSentiLabs = strtok.count_tokens();
			for (int k = 1; k < strtok.count_tokens(); k++) { // k=1, weil wir das eigentliche Wort auslassen und in der for-Schleife nur die Sentiment-Werte daf�r betrachten
				val = atof(strtok.token(k).c_str()); // Der String wird in ein double "val" konvertiert
				if (tmp < val) {
					tmp = val;
					labID = k-1; // Die labID gibt also das Sentiment mit dem h�chsten Wert an. Bei [0.05 0.9 0.05] w�rde somit labID=1 sein
				}
				wordPrior.push_back(val);
			}
			Word_Prior_Attr temp = {labID, wordPrior};  // sentiment label ID, sentiment label distribution
			sentiLex.insert(pair<string, Word_Prior_Attr >(strtok.token(0), temp));
		}
    }
    
	if (sentiLex.size() <= 0) {
		printf("Can not find any sentiment lexicon in file %s!\n", sentiLexiconFile.c_str());
		return 1;
	}
	
    fclose(fin);
    return 0;
}


int dataset::write_wordmap(string wordmapfile, mapword2atr &pword2atr) {

    FILE * fout = fopen(wordmapfile.c_str(), "w");
    if (!fout) {
		printf("Cannot open file %s to write!\n", wordmapfile.c_str());
		return 1;
    }    
    
    mapword2atr::iterator it;
	// Die erste Zeile in wordmap.txt ist also die Anzahl an W�rtern
    fprintf(fout, "%d\n", (int)(pword2atr.size())); // wieviele Worte gibt es insgesamt (vocabSize)
    for (it = pword2atr.begin(); it != pword2atr.end(); it++) {
		// wordmap.txt wird dann gef�llt mit dem Wort (it->first) und dem Wortindex (it->second.id) (an welcher Stelle kommt es zum ersten Mal vor)
	    fprintf(fout, "%s %d\n", (it->first).c_str(), it->second.id);
    }
    
    fclose(fout);
    return 0;
}

// Liest wordmap.txt ein. Die erste Zeile beschreibt numVocSize (Anzahl aller unterschiedlicher Vokabeln)
// Die folgenden Zeilen beschreiben Paare von Word-String und korrespondierender Word-ID f�r den gescannten Corpus
// z.B. "brutal 35"
// Hier wird eine mapid2word pid2word beschrieben
int dataset::read_wordmap(string wordmapfile, mapid2word &pid2word) {
    pid2word.clear(); 
    FILE * fin = fopen(wordmapfile.c_str(), "r");
    if (!fin) {
		printf("Cannot open file %s to read!\n", wordmapfile.c_str());
		return 1;
    }    
    
    char buff[BUFF_SIZE_SHORT];
    string line;
    
    fgets(buff, BUFF_SIZE_SHORT - 1, fin);
    int nwords = atoi(buff);
    
    for (int i = 0; i < nwords; i++) {
		fgets(buff, BUFF_SIZE_SHORT - 1, fin);
		line = buff;
		strtokenizer strtok(line, " \t\r\n");
		if (strtok.count_tokens() != 2) {
			printf("Warning! Line %d in %s contains less than 2 words!\n", i+1, wordmapfile.c_str());
			continue;
		}
		
		pid2word.insert(pair<int, string>(atoi(strtok.token(1).c_str()), strtok.token(0))); // Wort-ID (strtok.token(1)) und Wort (strtok.token(0)) werden gespeichert
    }
    
    fclose(fin);
    return 0;
}

// Hier wird dagegen eine mapword2id erstellt
int dataset::read_wordmap(string wordmapfile, mapword2id& pword2id) {
    pword2id.clear();
    char buff[BUFF_SIZE_SHORT];
    string line;


    FILE * fin = fopen(wordmapfile.c_str(), "r");
    if (!fin) {
		printf("Cannot read file %s!\n", wordmapfile.c_str());
		return 1;
    }    
        
    fgets(buff, BUFF_SIZE_SHORT - 1, fin);
    int nwords = atoi(buff);
    
    for (int i = 0; i < nwords; i++) {
		fgets(buff, BUFF_SIZE_SHORT - 1, fin);
		line = buff;
		strtokenizer strtok(line, " \t\r\n");
		if (strtok.count_tokens() != 2) {
			continue;
		}
		pword2id.insert(pair<string, int>(strtok.token(0), atoi(strtok.token(1).c_str())));
    }
    
    fclose(fin);
    return 0;
}

