// Copyright 2015 the Dart project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

void main() {
  int age = 20;
  double salary = 234.567;
  var fullName = "";
  
  fullName = "Golf";
  
  var helloTxt = "สวัสดีคุณ $fullName";
  
  print(fullName.runtimeType);
  print("My Name is ${fullName.toUpperCase()}");
  print(helloTxt);
  print(age);
  print(salary.toString());
  
  var isShow = true;
  bool isActive = false;
  
  var msg = isShow ? 'Hello True' : 'Hello False';
  print(msg);
  var msg2 = isActive == true ? 'Hello True' : 'Hello False';
  print(msg2);
}
