static bool switch_to_user(uid_i user_id,gid_t gp_id)
{
    if((user_id==0) && (gp_id==0))//确保目标用户不是root
        return false;
    
    gid_t gid=getgid();
    uid_t uid=getuid();
    if(((gid!=0) || (uid!=0)) && ((gid!=gp_id) || (uid!=user_id)))//确保当前用户合法：root 或者目标用户
        return false;
    
    if(uid!=0)//如果不是root，则已经是目标用户
        return true;
    
    if((setgid(gp_id)<0) || (setuid(user_id)<0))//切换到目标用户
        return false;
    
    return true;
}