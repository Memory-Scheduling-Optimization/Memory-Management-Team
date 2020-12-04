class MaxHeap {
public:
    int* arr;        //pointer to array of elements in heap
    int size;        //current number of elements in heap

    int parent(int child) {
	if(child % 2 == 0) return (child / 2) - 1;
        else return (child / 2);
    }
    
    int left(int parent) { return (2 * parent + 1); }
    
    int right(int parent) { return (2 * parent + 2); }
    
    bool isLeaf(int i) {
	if(i >= size) return true;
        return false;
    }
    
    void siftup(int i) {
	if(i == 0) return;     //only one element in the array
        int parent_index = parent(i);        //get the index of the parent
        if(arr[parent_index] < arr[i]) {
            int temp = arr[parent_index];   //if value at parent index is less than inserted value
            arr[parent_index] = arr[i];     //swap the values
            arr[i] = temp;
            siftup(parent_index);    // loop untill it satisfies parent child max heap relationship
        }
    }
    
    void siftdown(int i) {
	int l = left(i);
        int r = right(i);
        if(isLeaf(l)) return;
        int maxIndex = i;
        if(arr[l] > arr[i]) maxIndex = l;
        if(!isLeaf(r) && (arr[r] > arr[maxIndex])) maxIndex = r;
        if(maxIndex != i) {
            int temp = arr[i];
            arr[i] = arr[maxIndex];
            arr[maxIndex] = temp;
            siftdown(maxIndex);
        }
    }
    
    MaxHeap(int capacity) {
        arr = new int[capacity];
        size = 0;
    }

    int getSize() { return size; }
    
    int getMax() { return arr[0]; } //maximum value will be at index 0
    
    void insert(int k) {
	arr[size] = k;   //insert the value into array
        siftup(size);
        size++;     //increment the size of the array
    }
    
    int extractMax() {
	int max = arr[0];
        arr[0] = arr[size - 1];
        size--;
        siftdown(0);
        return max;
    }
    
    int removeAt(int k) {
	int r = arr[k];
	arr[k] = arr[size - 1];  //replace with rightmost leaf
        size--;
        int p = parent(k);
        if(k == 0 || arr[k] < arr[p]) siftdown(k);
        else siftup(k);
        return r;
    }
    
    void heapify(int *array, int len) {
	size = len;
        arr = array;
	for(int i = size - 1; i >= 0; --i) siftdown(i);
    }
};
