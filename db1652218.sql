use db1652218;

drop table if exists loginfo;
drop table if exists message;
drop table if exists user;

create table user(
    id int unsigned primary key auto_increment,
    name varchar(28) not null,
    passwd varchar(32),
    msgnum int unsigned not null default 0,
    unique key(name)
);

create table message(
    id int unsigned primary key auto_increment,
    sid int unsigned not null,
    rid int unsigned not null,
    content varchar(1024),
    sendtime bigint not null,
    foreign key(sid) references user(id),
    foreign key(rid) references user(id)
);

create table loginfo(
    uid int unsigned not null,
    sockfd int not null,
    status tinyint(1) unsigned not null default 0,
    foreign key(uid) references user(id),
    unique key(sockfd)
);

insert into user (name) values ('all');
insert into user (name) values ('herry');
insert into user (name) values ('percy');
insert into user (name) values ('richael');
insert into user (name) values ('¶ÎÎ¡');
insert into user (name) values ('dw');
insert into user (name) values ('·½ºÍÒ×');
insert into user (name) values ('fhy');
insert into user (name) values ('³Â°ØÓğ');
insert into user (name) values ('cby');

insert into message (sid,rid,content,sendtime) values (5,9,'¶ÎÎ¡@³Â°ØÓğ:Ò»',1545530780);
insert into message (sid,rid,content,sendtime) values (9,5,'³Â°ØÓğ@¶ÎÎ¡:¶ş',1545530781);
insert into message (sid,rid,content,sendtime) values (5,9,'¶ÎÎ¡@³Â°ØÓğ:Èı',1545530782);
insert into message (sid,rid,content,sendtime) values (9,5,'³Â°ØÓğ@¶ÎÎ¡:ËÄ',1545530783);
insert into message (sid,rid,content,sendtime) values (5,9,'¶ÎÎ¡@³Â°ØÓğ:Îå',1545530784);
insert into message (sid,rid,content,sendtime) values (9,5,'³Â°ØÓğ@¶ÎÎ¡:Áù',1545530785);
insert into message (sid,rid,content,sendtime) values (5,9,'¶ÎÎ¡@³Â°ØÓğ:Æß',1545530786);
insert into message (sid,rid,content,sendtime) values (9,5,'³Â°ØÓğ@¶ÎÎ¡:°Ë',1545530787);
insert into message (sid,rid,content,sendtime) values (5,9,'¶ÎÎ¡@³Â°ØÓğ:¾Å',1545530788);
insert into message (sid,rid,content,sendtime) values (9,5,'³Â°ØÓğ@¶ÎÎ¡:Ê®',1545530789);

insert into message (sid,rid,content,sendtime) values (5,1,'¶ÎÎ¡@all:Ò»',1545530780);
insert into message (sid,rid,content,sendtime) values (9,1,'³Â°ØÓğ@all:¶ş',1545530781);
insert into message (sid,rid,content,sendtime) values (5,1,'¶ÎÎ¡@all:Èı',1545530782);
insert into message (sid,rid,content,sendtime) values (9,1,'³Â°ØÓğ@all:ËÄ',1545530783);
insert into message (sid,rid,content,sendtime) values (5,1,'¶ÎÎ¡@all:Îå',1545530784);
insert into message (sid,rid,content,sendtime) values (9,1,'³Â°ØÓğ@all:Áù',1545530785);
insert into message (sid,rid,content,sendtime) values (5,1,'¶ÎÎ¡@all:Æß',1545530786);
insert into message (sid,rid,content,sendtime) values (9,1,'³Â°ØÓğ@all:°Ë',1545530787);
insert into message (sid,rid,content,sendtime) values (5,1,'¶ÎÎ¡@all:¾Å',1545530788);
insert into message (sid,rid,content,sendtime) values (9,1,'³Â°ØÓğ@all:Ê®',1545530789);

update user set msgnum=7;