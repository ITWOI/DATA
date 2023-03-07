; ModuleID = 'diffCases.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [3 x i8] c"aa\00", align 1
@.str1 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@.str2 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@.str3 = private unnamed_addr constant [3 x i8] c"1\0A\00", align 1
@.str4 = private unnamed_addr constant [3 x i8] c"0\0A\00", align 1
@g_p = common global i8* null, align 8

; Function Attrs: nounwind uwtable
define void @strFun(i8* %p) #0 {
entry:
  %p.addr = alloca i8*, align 8
  store i8* %p, i8** %p.addr, align 8
  %0 = load i8** %p.addr, align 8
  %call = call i8* @strcat(i8* %0, i8* getelementptr inbounds ([3 x i8]* @.str, i32 0, i32 0)) #3
  ret void
}

; Function Attrs: nounwind
declare i8* @strcat(i8*, i8*) #1

; Function Attrs: nounwind uwtable
define void @funPointer1() #0 {
entry:
  %p1 = alloca i8*, align 8
  %p2 = alloca i8*, align 8
  %call = call noalias i8* @malloc(i64 10) #3
  store i8* %call, i8** %p1, align 8
  %0 = load i8** %p1, align 8
  %call1 = call i8* @strcpy(i8* %0, i8* getelementptr inbounds ([6 x i8]* @.str1, i32 0, i32 0)) #3
  %1 = load i8** %p1, align 8
  store i8* %1, i8** %p2, align 8
  %2 = load i8** %p2, align 8
  %call2 = call i8* @strcpy(i8* %2, i8* getelementptr inbounds ([6 x i8]* @.str2, i32 0, i32 0)) #3
  %3 = load i8** %p2, align 8
  call void @strFun(i8* %3)
  %4 = load i8** %p1, align 8
  call void @free(i8* %4) #3
  store i8* null, i8** %p1, align 8
  %5 = load i8** %p2, align 8
  %cmp = icmp eq i8* %5, null
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %call3 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str3, i32 0, i32 0))
  br label %if.end

if.else:                                          ; preds = %entry
  %call4 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str4, i32 0, i32 0))
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) #1

; Function Attrs: nounwind
declare i8* @strcpy(i8*, i8*) #1

; Function Attrs: nounwind
declare void @free(i8*) #1

declare i32 @printf(i8*, ...) #2

; Function Attrs: nounwind uwtable
define void @funPointer2() #0 {
entry:
  %n = alloca i32, align 4
  %p1 = alloca i8*, align 8
  store i32 0, i32* %n, align 4
  %call = call noalias i8* @malloc(i64 10) #3
  store i8* %call, i8** %p1, align 8
  %0 = load i8** %p1, align 8
  %call1 = call i8* @strcpy(i8* %0, i8* getelementptr inbounds ([6 x i8]* @.str1, i32 0, i32 0)) #3
  %1 = load i8** %p1, align 8
  store i8* %1, i8** @g_p, align 8
  %2 = load i8** @g_p, align 8
  %call2 = call i8* @strcpy(i8* %2, i8* getelementptr inbounds ([6 x i8]* @.str2, i32 0, i32 0)) #3
  %3 = load i8** @g_p, align 8
  call void @strFun(i8* %3)
  %4 = load i8** @g_p, align 8
  %5 = load i8* %4, align 1
  %6 = load i8** %p1, align 8
  store i8 %5, i8* %6, align 1
  %7 = load i8** %p1, align 8
  call void @free(i8* %7) #3
  store i8* null, i8** %p1, align 8
  %8 = load i8** @g_p, align 8
  %cmp = icmp eq i8* %8, null
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %call3 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str3, i32 0, i32 0))
  br label %if.end

if.else:                                          ; preds = %entry
  %call4 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str4, i32 0, i32 0))
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

; Function Attrs: nounwind uwtable
define void @funPointer3() #0 {
entry:
  %p1 = alloca i8*, align 8
  %p2 = alloca i8*, align 8
  %call = call noalias i8* @malloc(i64 10) #3
  store i8* %call, i8** %p1, align 8
  %0 = load i8** %p1, align 8
  %call1 = call i8* @strcpy(i8* %0, i8* getelementptr inbounds ([6 x i8]* @.str1, i32 0, i32 0)) #3
  %1 = load i8** %p1, align 8
  store i8* %1, i8** %p2, align 8
  %2 = load i8** %p2, align 8
  %call2 = call i8* @strcpy(i8* %2, i8* getelementptr inbounds ([6 x i8]* @.str2, i32 0, i32 0)) #3
  %3 = load i8** %p2, align 8
  call void @strFun(i8* %3)
  %4 = load i8** %p2, align 8
  %5 = load i8* %4, align 1
  %6 = load i8** %p1, align 8
  store i8 %5, i8* %6, align 1
  %7 = load i8** %p1, align 8
  call void @free(i8* %7) #3
  store i8* null, i8** %p1, align 8
  %8 = load i8** %p2, align 8
  %cmp = icmp eq i8* %8, null
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %call3 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str3, i32 0, i32 0))
  br label %if.end

if.else:                                          ; preds = %entry
  %call4 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str4, i32 0, i32 0))
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

; Function Attrs: nounwind uwtable
define void @notMod() #0 {
entry:
  %p1 = alloca i8*, align 8
  %p2 = alloca i8**, align 8
  %call = call noalias i8* @malloc(i64 10) #3
  store i8* %call, i8** %p1, align 8
  store i8** %p1, i8*** %p2, align 8
  %0 = load i8** %p1, align 8
  call void @strFun(i8* %0)
  %1 = load i8*** %p2, align 8
  %2 = load i8** %1, align 8
  call void @strFun(i8* %2)
  %3 = load i8*** %p2, align 8
  %4 = load i8** %3, align 8
  call void @free(i8* %4) #3
  %5 = load i8*** %p2, align 8
  store i8* null, i8** %5, align 8
  %6 = load i8** %p1, align 8
  %cmp = icmp eq i8* %6, null
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %call1 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str3, i32 0, i32 0))
  br label %if.end

if.else:                                          ; preds = %entry
  %call2 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str4, i32 0, i32 0))
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret void
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %p1 = alloca i8*, align 8
  %p2 = alloca i8*, align 8
  store i32 0, i32* %retval
  %call = call noalias i8* @malloc(i64 32) #3
  store i8* %call, i8** %p1, align 8
  %0 = load i8** %p1, align 8
  %call1 = call i8* @strcpy(i8* %0, i8* getelementptr inbounds ([6 x i8]* @.str1, i32 0, i32 0)) #3
  %1 = load i8** %p1, align 8
  store i8* %1, i8** %p2, align 8
  %2 = load i8** %p2, align 8
  %call2 = call i8* @strcpy(i8* %2, i8* getelementptr inbounds ([6 x i8]* @.str2, i32 0, i32 0)) #3
  %3 = load i8** %p1, align 8
  call void @free(i8* %3) #3
  %4 = load i8** %p2, align 8
  %cmp = icmp eq i8* %4, null
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %call3 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str3, i32 0, i32 0))
  br label %if.end

if.else:                                          ; preds = %entry
  %call4 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([3 x i8]* @.str4, i32 0, i32 0))
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  call void @funPointer1()
  call void @funPointer2()
  call void @funPointer3()
  call void @notMod()
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
