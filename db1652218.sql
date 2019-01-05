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
insert into user (name) values ('��Ρ');
insert into user (name) values ('dw');
insert into user (name) values ('������');
insert into user (name) values ('fhy');
insert into user (name) values ('�°���');
insert into user (name) values ('cby');

insert into message (sid,rid,content,sendtime) values (5,9,'��Ρ@�°���:һ',1545530780);
insert into message (sid,rid,content,sendtime) values (9,5,'�°���@��Ρ:��',1545530781);
insert into message (sid,rid,content,sendtime) values (5,9,'��Ρ@�°���:��',1545530782);
insert into message (sid,rid,content,sendtime) values (9,5,'�°���@��Ρ:��',1545530783);
insert into message (sid,rid,content,sendtime) values (5,9,'��Ρ@�°���:��',1545530784);
insert into message (sid,rid,content,sendtime) values (9,5,'�°���@��Ρ:��',1545530785);
insert into message (sid,rid,content,sendtime) values (5,9,'��Ρ@�°���:��',1545530786);
insert into message (sid,rid,content,sendtime) values (9,5,'�°���@��Ρ:��',1545530787);
insert into message (sid,rid,content,sendtime) values (5,9,'��Ρ@�°���:��',1545530788);
insert into message (sid,rid,content,sendtime) values (9,5,'�°���@��Ρ:ʮ',1545530789);

insert into message (sid,rid,content,sendtime) values (5,1,'��Ρ@all:һ',1545530780);
insert into message (sid,rid,content,sendtime) values (9,1,'�°���@all:��',1545530781);
insert into message (sid,rid,content,sendtime) values (5,1,'��Ρ@all:��',1545530782);
insert into message (sid,rid,content,sendtime) values (9,1,'�°���@all:��',1545530783);
insert into message (sid,rid,content,sendtime) values (5,1,'��Ρ@all:��',1545530784);
insert into message (sid,rid,content,sendtime) values (9,1,'�°���@all:��',1545530785);
insert into message (sid,rid,content,sendtime) values (5,1,'��Ρ@all:��',1545530786);
insert into message (sid,rid,content,sendtime) values (9,1,'�°���@all:��',1545530787);
insert into message (sid,rid,content,sendtime) values (5,1,'��Ρ@all:��',1545530788);
insert into message (sid,rid,content,sendtime) values (9,1,'�°���@all:ʮ',1545530789);

update user set msgnum=7;