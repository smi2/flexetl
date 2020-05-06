<?php
mkdir('/tmp/flexEtlTest/');
mkdir('/tmp/flexEtlTest/TableViews');
mkdir('/tmp/flexEtlTest/TableClicks');

for($f=0;$f<20000;$f++)
{
    $fnTableViews=date('YmdHis').mt_rand(10,20).'.json'; // generate many files
    file_put_contents('/tmp/flexEtlTest/TableViews/'.$fnTableViews,json_encode(
        [
            'dt'=>time(),
            'type'=>'view',
            'date'=>date('Y-m-d'),
            'count'=>$f,
            'hash'=>md5($f),
            'a'=>'AAA',
            'b'=>'BBBB'
        ]
    ));

    $fnTableClick=date('YmdHis').mt_rand(10,20).'.json'; // generate many files
    file_put_contents('/tmp/flexEtlTest/TableClicks/'.$fnTableClick,json_encode(
        [
            'dt'=>time(),
            'type'=>'click',
            'date'=>date('Y-m-d'),
            'count'=>$f,
            'source'=>md5($f),
            'zzz'=>'AAA',
            'sss'=>'BBBB'
        ]
    ));
}