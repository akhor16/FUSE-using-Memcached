# FUSE using Memcached
Our main problem is to represent file system as key-value pairs. We have directories and files, the size of each may be gigabytes, but according to memcached each value for given key may only be 1MB.  

##  Main Mechanism:
There is 2 main files: 1 of them helps us to work with memcached, second file implements FUSE system calls with using the first file.
### First file:
In memcached.h, there are method headers to work with FUSE. Also a structure, which is known for both sides.
For any value, the first thing we see there is a structure, where the metadata is held.
```
struct bucket_struct {
	int cur_size;

};
```
memcached.h - parameters:
#define SPACE_FOR_STRUCTS 200 -  space needed for structs in buckets
#define MAX_BUCKET_SIZE 1024 - bucket size per key for files.ფაილებისთვის თითოეულ კალათაში data-ს ზომა.
#define FAST_MODE 0 (default 0) - when fast mode is 1, the program doesn't wait for STORED messages, which is faster but risky.


### FUSE side
Overriden FUSE methods for the project
## Directories and files
For any directory or a file, we have main and secondary key-values.
in particular: main part's key represents full-path of a file or directory. value for the mentioned key holds main information of the file or directory, which is a another structure (written down), but before this structure we have the aforementioned structure. 
```
struct header_struct {
	int is_dir; // 0 = file, 1 = dir, 2 = xattr
	int data_len; // length of real data	
	int num_buckets; // number of child buckets
	int parent_index; // index for the current file/directory in parent directories children.
	int xattr_num; //number of extended attributes
	
};
```

For both is_dir=0 and is_dir=1 we have num_buckets, where we keep number of buckets for the current file or directory. To get a specific bucket: concatatenate a number of the needed bucket(1 for the first bucket) and the full path of a file or directory. In each bucket(key starts with number) for the directories: there is kept full-path of the inner directory, for the files: data.


















# FUSE using Memcached
Our main problem is to represent file system as key-value pairs. We have directories and files, the size of each may be gigabytes, but according to memcached each value for given key may only be 1MB.  
ჩვენი ძირითადი ამოცანაა ფაილური სისტემა წარმოვადგინოთ key-value სახით, გვაქვს დირექტორიები და ფაილები, რომელთა ზომაც შეიძლება საკმაოდ დიდი იყოს, ხოლო memcached-ის მუშაობის პრინციპიდან გამომდინარე key-ს შეკითხვისას მთლიანი value იგზავნება ქსელში(ამის გარდა value-ს ზომა ისედაც შეზღუდულია მემქეშდის მიერ).
## ძირითადი მექანიზმი / Main Mechanism:
გვაქვს ორი ძირითადი ფაილი: ერთი, რომელიც მემქეშდთან მარტივად ურთიერთობის საშუალებას გვაძლევს და მეორე, რომელიც პირველის გამოყენებით ამუშავებს FUSE-ს სისტემქოლებს.
There is 2 main files: 1 of them helps us to work with memcached, second file implements FUSE system calls with using the first file.
### MEMCACHED-ის მხარე / First file:
გვაქვს memcached.h-ში აღწერილი მეთოდები რომელსაც FUSE იყენებს, ასევე სტრუქტურა, რომელიც ორივე მხარისთვის ცნობილია.
ნებისმიერი value-ს პირველი წევრია სტრუქტურა, რომელიც წარმოადგენს ერთ ინტს(მთლიანი value-ს ზომა ბაიტებში ამავე სტრუქტურის ჩათვლით). (რატომ სტრუქტურა და არა უბრალოდ ინტი? იმიტომ რომ შემდეგ შეცვლა, რამის დამატება ძალიან მარტივი იქნება).  
In memcached.h, there are method headers to work with FUSE. Also a structure, which is known for both sides.
For any value, the first thing we see there is a structure, where the metadata is held.
```
struct bucket_struct {
	int cur_size;

};
```
memcached.h -ში დატოვებულია პარამეტრების სამართავი რიცხვები:
#define SPACE_FOR_STRUCTS 200 სტრუქტურებისთვის საჭირო მეხსიერება კალათებში.
#define MAX_BUCKET_SIZE 1024 ფაილებისთვის თითოეულ კალათაში data-ს ზომა.
#define FAST_MODE 0 (default 0) ფაილში ჩაწერის ტესტი ბევრად სწრაფია 1ის შემთხვევაში, რადგანაც STORED მესიჯებს არ ველოდებით, რამაც შეიძლება პრობლემები გამოიწვიოს

### FUSE-ის მხარე / FUSE side
გადატვირთულია ამ პროექტისთვის საჭირო FUSE-ის მეთოდები
Overriden FUSE methods for the project
## დირექტორიებისა და ფაილების აგებულება / Directories and files
ნებისმიერი დირექტორიისა თუ ფაილისთვის გვაქვს მთავარი და ქვემდებარე key-value-ები,
კერძოდ: მთავარ ნაწილის key-ს წარმოადგენს full-path კონკრეტული ფაილისა თუ დირექტორიისა, ხოლო value ზოგად ინფორმაციას ამ ფაილსა თუ დირექტორიაზე, რომელიც ცალკე სტრუქტურაა(არ დაგვავიწყდეს ამ სტრუქტურამდე ზემოთ, მემქეშდში აღწერილი სტრუქტურა გვაქვს), ამ სტრუქტურის პირველ ელემეტნს წარმოადგენს isDir, რომელიც 1 არის დირექტორიის შემთხვევაში, ხოლო 0 ფაილებისთვის.
ეს სტრუქტურა შემდგომში დამატებითი ატრიბუტებისთვისაც გამოვიყენე და is_dir 2ია მაგ შემთხვევაში.  
For any directory or a file, we have main and secondary key-values.
in particular: main part's key represents full-path of a file or directory. value for the mentioned key holds main information of the file or directory, which is a another structure (written down), but before this structure we have the aforementioned structure. 
```
struct header_struct {
	int is_dir; // 0 = file, 1 = dir, 2 = xattr
	int data_len; // length of real data	
	int num_buckets; // number of child buckets
	int parent_index; // index for the current file/directory in parent directories children.
	int xattr_num; //number of extended attributes
	
};
```
ორივე შემთხვევაში გვაქვს num_buckets-რომლითაც ვინახავთ დანარჩენი კალათების რაოდენობას ამ კონკრეტული ფაილისა თუ დირექტორიისთვის, კონკრეტული ბაქეთის მიღებისთვის კი key-ს თავში ვაწებებთ ნომერს(1-დან დაწყებულს), დირექტორიებისთვის თითოელ დამხმარე ბაქეთში ინახება თითო შვილის full-path, ხოლო ფაილებისთვის data.
ფაილების შემთხვევაში თუ ფაილის დატაა შვილეულ ბაქეთში, მაშინ შედგენილ key-ს წინ ვუმატებთ სიმბოლო &-ს, ხოლო თუ დამატებითი ატრიბუტია სიმბოლო $-ს, მათი რაოდენობები კი ცალ-ცალკე ითვლება.
parent_index არის მშობელი დირექტორიის რომელ კალათაში ვინახავთ ამ კონკრეტული დირექტორიისა თუ ფაილის full-path-ს, ეს გვჭირდება წაშლის მექანიზმისთვის, რომლის ზოგადი ალგორითმი ასეთია: ამ დირექტორიის ბოლო ელემენტს  და გადავსვამთ წასაშლელი ელემენტის ადგილას, ანუ key იგივეს ვუტოვებთ რაც წასაშლელს ჰქონდა, ხოლო value-ბოლო ელემენტის გადმოგვაქვს(რომელიც უბრალოდ full-path-ია (რა თქმა უნდა სტრუქტურით)), შემდეგ კი ბოლო ელემენტს ვშლით, გადმოტანილი ფაილის/dir-ის parent_index-საც ვანახლებთ. ასეთივეა ატრიბუტების წასაშლელი მექანიზმიც.
ატრიბუტების value: თავიდან, ისევე როგორც ყველგან, გვაქვს bucket_Struct, შემდეგ მოსდევს ეგრევე header_struct, შემდეგ int, რომელშიც წერია ამ ატრიბუტის სახელის სიგრძე, შემდეგ სახელი, შემდეგ int, რომელშიც value-ს სიგრძე წერია ბაიტებში და შემდეგ მოსდევს value.
უნდა აღინიშნოს რომ გვაქვს ცალკეული key:value; key = "stats", სადაც ვინახავთ ზოგად ინფორმაციას ფაილური სისტემის შესახებ.  

For both is_dir=0 and is_dir=1 we have num_buckets, where we keep number of buckets for the current file or directory. To get a specific bucket: concatatenate a number of the needed bucket(1 for the first bucket) and the full path of a file or directory. In each bucket(key starts with number) for the directories: there is kept full-path of the inner directory, for the files: data.

```
struct stats_struct {
	int total_bytes; //ბაიტების მთლიანი რაოდენობა
	int block_size; // ფაილების ბლოკის ზომა(ეს გვჭირდება საწყისი შემოწმებისთვის)

};
```
