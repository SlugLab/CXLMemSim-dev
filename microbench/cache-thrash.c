int main(){
	volatile char buf[16*1024*3];
	for(int i = 0; i< 0xffffffff; i++){
		int j = 1024*3 * (i%16);
		buf[j] += 1;
	}
	for(int i = 0; i< 0xffffffff; i+=2){
		int j = 1024*3 * (i%16);
		int k = 1024*3 * (i+1%16);

		buf[j] += 1;
		buf[k] += 1;
	}
	return 0;
}
