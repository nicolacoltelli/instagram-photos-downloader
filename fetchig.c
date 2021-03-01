#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./curl/curl.h"
#include "./curl/easy.h"
#include <sys/stat.h>

#define BUFFER 1024
#define PRINT_SOURCE 0

#define MAX_IMAGES_COUNT 128

#define INSTAGRAM_URL "https://www.instagram.com/"
#define INSTAGRAM_URL_LEN 26

#define PROFILE_PIC_PREFIX "\"profile_pic_url_hd\":\""
#define PROFILE_PIC_PREFIX_LEN 22

#define POST_PIC_PREFIX "\"display_url\":\""
#define POST_PIC_PREFIX_LEN 15


struct MemoryStruct {
	char *memory;
	size_t size;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
size_t WriteToFile(void *contents, size_t size, size_t nmemb, void *userp);

void* xmalloc(int bytes);

int main(int argc, char *argv[]){

	char* user_id = NULL;
	char* user_url = NULL;

	if (argc != 2){
		printf("Invalid number of arguments.\n");
		printf("USAGE: instagram_photos_downloader username.\n");
		exit(1);
	}

	char temp[BUFFER];
	memset(temp, '\0', sizeof(char) * BUFFER);

	if (sscanf(argv[1], "%s", temp)!=1){
		printf("Invalid argument. Must be a string.\n");
		exit(1);
	}

	//Allocate space for id and url
	int user_id_len = strlen(temp);

	//+1 because of last slash character
	int url_len = INSTAGRAM_URL_LEN + user_id_len + 1;

	user_id = xmalloc(sizeof(char) * (user_id_len + 1) );
	user_url = xmalloc(sizeof(char) * (url_len + 1) );

	memset(user_id, '\0', sizeof(char) * (user_id_len + 1) );
	memset(user_url, '\0', sizeof(char) * (url_len + 1) );

	//Input id
	strncpy(user_id, temp, user_id_len + 1);

	//Generate url from id
	strncpy(user_url, INSTAGRAM_URL, INSTAGRAM_URL_LEN);
	strncat(user_url, user_id, user_id_len);
	strncat(user_url, "/", 1);


	//Debug info
	printf("ID: %s\n", user_id);
	printf("Profile link: %s\n\n", user_url);


	CURL *curl;
	CURLcode res;

	//The response from the request will be written here
	struct MemoryStruct source;
	source.memory = xmalloc(1);
	source.size = 0;

	//Initialize the environment needed by curl
	curl_global_init(CURL_GLOBAL_ALL);

	//Initialize curl session
	//This session will get the source of the page
	curl = curl_easy_init();

	if(curl) {
		//Specify the URL to get
		curl_easy_setopt(curl, CURLOPT_URL, user_url);

		//Turn off info printed on screen
		//curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

		// send all data to this function
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

		// we pass our 'source' struct to the callback function
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &source);

		// Perform the request, res will get the return code
		res = curl_easy_perform(curl);

		// Check for errors
		if(res != CURLE_OK){
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		}
		/* always cleanup */
		curl_easy_cleanup(curl);

	}


	free(user_url);


	//Print the source page
	if (PRINT_SOURCE){
		for (int i = 0; i<source.size; i++){
			printf("%c", (char)source.memory[i]);
		}
		printf("\n");
	}

	//Pointer to source. Will be used to read sequentially the source
	char* ptr = NULL;

	//Check if user exists
	if ( ptr = strstr(source.memory, "<title>") ){
		
		//@remove
		/*
		for (int i = 0; i<26; i++){
			ptr++;
		}
		*/

		while ( *ptr == ' '){
			++ptr;
		}

		if ( strncmp(ptr, "Page Not Found", 14) == 0 ){
			printf("User does not exist.\n");
			free(source.memory);
			free(user_id);
			exit(2);
		}
	} else {
		printf("Unable to retrieve web page.\n");
		free(source.memory);
		free(user_id);
		exit(3);
	}


	// ----- From there we parse the source to find the links of the pic sources -----

	//Links[0] will contain the profile profile picture
	//Links[1..n<=MAX_IMAGES_COUNT] will contain the posts' image.
	char* links[MAX_IMAGES_COUNT];
	for (int i = 0; i<MAX_IMAGES_COUNT; i++){
		links[i] = NULL;
	}

	//Parameters (initialized for the profile pic, will change accordingly for the posts)
	char* token = xmalloc(sizeof(char) * (PROFILE_PIC_PREFIX_LEN + 1) );
	memset(token, '\0', sizeof(char) * (PROFILE_PIC_PREFIX_LEN + 1) );
	strncpy(token, PROFILE_PIC_PREFIX, PROFILE_PIC_PREFIX_LEN);
	int token_length = PROFILE_PIC_PREFIX_LEN;

	int images_found_count = 0;
	while ( (images_found_count < MAX_IMAGES_COUNT) &&
		 		( (ptr = strstr(ptr, token) ) != NULL ) ) {

		char temp_image_url[BUFFER];
		memset(temp_image_url, '\0', sizeof(char) * BUFFER);
		int dim = 0;

		//Ignore the string we looked for
		ptr += token_length;
		/* @remove
		for (int i = 0; i<skip; i++){
			ptr++;
		}
		*/

		//Save char by char until we find the closing quotation marks, or the source ends
		while( ptr != NULL  &&  *ptr!= '\"' ){

			//extra check because IG wants '&' to become '\u0026' or vice versa, randomly

			if (*ptr=='&'){

				strncpy(temp_image_url + dim, "\\u0026", 6);
				dim += 6;
				++ptr;

				/* @remove
				temp[dim++] = '\\';
				temp[dim++] = 'u';
				temp[dim++] = '0';
				temp[dim++] = '0';
				temp[dim++] = '2';
				temp[dim] = '6';
				*/

			} //else if (*ptr=='\\' && *(ptr+1)=='u' && *(ptr+2)=='0' && *(ptr+3)=='0' && *(ptr+4)=='2' && *(ptr+5)=='6'){
				else if ( strncmp(ptr, "\\u0026", 6) == 0 ) {

				temp_image_url[dim] = '&';
				++dim;
				ptr += 6;

			} else {

				temp_image_url[dim] = *ptr;
				++dim;
				++ptr;

			}

		}

		links[images_found_count] = xmalloc(sizeof(char) * dim + 1);
		strncpy(links[images_found_count], temp_image_url, dim);
		links[images_found_count][dim] = '\0';

		//Change parameters after first iteration
		if (0 == images_found_count){
			free(token);
			token = xmalloc(sizeof(char) * (POST_PIC_PREFIX_LEN + 1) );
			memset(token, '\0', sizeof(char) * (POST_PIC_PREFIX_LEN + 1) );
			strncpy(token, POST_PIC_PREFIX, POST_PIC_PREFIX_LEN);
			token_length = POST_PIC_PREFIX_LEN;
		}

		++images_found_count;

	}
	free(token);

	//Print urls
	for (int i = 0; i < images_found_count; ++i){
			printf("%s\n", links[i]);
	}
	printf("\n");


	// ----- Now we will write to file the images using the URLs we parsed

	//Generate directory
	//0777 means we give permissions to execute, write, read
	//to owner, owner's group and everybody else. (widest possible access)
	mkdir(user_id, 0777);

	int max_digits = 1;
	int images_found_count_temp = images_found_count;
	while (images_found_count_temp /= 10) ++max_digits; 

	for (int current_image_index = 0; current_image_index < images_found_count; ++current_image_index){

		curl = curl_easy_init();

			//Check for null urls
			if(curl && links[current_image_index]) {

				//Generate filename ( "./$id/$progressive_number.jpg" )
				//filename_length = ./ + id + / + progressive_number + .jpg + NULL terminator.
				int filename_length = 2 + user_id_len + 1 + max_digits + 4 + 1;
				char* filename = xmalloc(sizeof(char) * filename_length);
				memset(filename, '\0', sizeof(char) * filename_length);

				strncat(filename, "./", 2);
				strncat(filename, user_id, user_id_len);
				strncat(filename, "/", 1);

				//Generate filename progressively, appending current_image_index at the end of the path
				int current_image_index_temp = current_image_index;
				for (int digit_index = max_digits-1; digit_index >= 0; --digit_index){
					filename[2 + user_id_len + 1 + digit_index] = (char)((current_image_index_temp % 10) + 48);
					current_image_index_temp /= 10;
				}

				strncat(filename, ".jpg", 4);

				printf("%s\n", filename);

				//Specify the URL to get
				curl_easy_setopt(curl, CURLOPT_URL, links[current_image_index]);

				FILE* fPtr = NULL;

				//File is opened in write binary mode since it will save an image
				if ( (fPtr = fopen(filename, "wb")) != NULL ){

					// send all data to this function
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);

					// we pass the file to the WriteToFile function
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, fPtr);

					// Perform the request, res will get the return code
					res = curl_easy_perform(curl);

					// Check for errors
					if(res != CURLE_OK){
						fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
					}

				}

				fclose(fPtr);
				free(filename);

			}

			// always cleanup
			curl_easy_cleanup(curl);

	}

	//Clean the environment initialized by curl_global_init()
	curl_global_cleanup();

	//Free memory
	free(user_id);

	for (int i = 0; i<images_found_count; i++){
		if (links[i]){
			free(links[i]);
		}
	}

	free(source.memory);

	return 0;

}

void* xmalloc(int bytes){
	void* ptr;
	if ( (ptr = malloc(bytes) ) == NULL){
		printf("Out of memory\n");
		exit(1);
	}
	return ptr;
}

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp){

	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;

}

size_t WriteToFile(void *contents, size_t size, size_t nmemb, void *userp){
	size_t written;
	written = fwrite(contents, size, nmemb, userp);
	return written;
}
